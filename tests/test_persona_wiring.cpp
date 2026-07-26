//
// tests/test_persona_wiring.cpp
//
// macro-history-v1 H2 steps 2b + 2c AND H3 wiring: the persona-timeline
// acceptance gates (docs/h2_persona_timeline.md +
// docs/h3_mortality_estate.md, authority U-7 + addendum).
// test_persona_timeline and test_lifespan pin the primitives' MEANING;
// these gates pin that generation REALIZES them.
//
//   2b (income switch) — one income-only GateWorld (300 people, 4 years
//   at 1991): retiree silence, salary stop, student/SSA onsets, full
//   arcs, revenue stop, post-close payroll.
//
//   2c — three additions on the same worlds:
//     A. END-OF-WINDOW PERSONA: resolveEndOfWindowPersonas (the seam
//        both AML streaming sinks call from takeArtifacts) echoes
//        personaAt(corpus end) exactly; empty streams and packs
//        without the timeline lane leave the seed assignment.
//     B. PAYDAY RE-ANCHOR: buildPaydaysByPerson screens by
//        isPaydayInbound, which admits the SSA channels — so a
//        mid-window retiree's liquidity cycle re-anchors from salary
//        days to SSA deposit days with NO further wiring. Pinned here
//        against regression.
//     C. RETIREMENT SPENDING STEP: a second GateWorld with base
//        routines runs the real spending simulator; the in-window
//        retiree cohort's mean ticket drops relative to a
//        non-retiring salaried control (difference-in-differences
//        around each claiming day; Aguiar-Hurst ~-12% + the SSA-income
//        liquidity response, generous band).
//
//   H3 — death wiring, in two parts on the same worlds:
//     INCOME (income world): the timeline carries tl.death from the
//     {"mortality"} lane; no paycheck after death (+10d posting
//     grace), no SSA deposit at/after death, no revenue past the
//     death month (+45d grace); in-window deaths EXIST and >=1
//     decedent had prior income.
//     SPENDING (spending world): the emission loop stops a spender's
//     person-days at their death day — ZERO spending rows sourced
//     from a dead spender on or after it; >=1 in-window-dead spender
//     spent while alive (the stop is realized). Funeral/estates and
//     recurring close-out land in the next H3 round (declared
//     interim: rent/bills/subscriptions still bill the estate).
//
// This gate's FIRST run (2b) exposed a PRE-EXISTING era-axis defect:
// the weekly/biweekly paydate lattices only existed at or after their
// 2025 parity anchor, so 75% of employer profiles were silent in
// every pre-2025 window (fixed in activity/recurring/payroll.hpp).
// The [diag] blocks that localized it are retained. The first run
// also mis-gated RETIREE-seed revenue: a seed retiree's
// investment-style revenue is legitimately perpetual, so the revenue
// gate mirrors generation exactly (retiree/HNW exempt from the
// persona gate — but NOT from the death gate).
//

#include "phantomledger/activity/spending/actors/spender.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/synth/ids.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/legit/blueprints/paydays.hpp"

#include "gate_world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace pl = ::PhantomLedger;
namespace tlx = pl::synth::personas::timeline;
namespace channels = pl::channels;
namespace amlv = pl::exporter::aml::vertices;

using PersonaType = pl::personas::Type;
using pltest::GateWorld;
using pltest::WorldSpec;

namespace {

constexpr std::uint64_t kSeed = 424242;
constexpr std::int32_t kPopulation = 300;
constexpr int kDays = 1460; // four years (income-only 2b world)

// The 2c spending world: base routines + the real spending simulator.
constexpr int kSpendDays = 1095; // three years
constexpr std::uint32_t kCohortMarginDays = 120;
constexpr std::uint32_t kSplitBufferDays = 14;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

struct PersonRows {
  std::int64_t firstSalary = 0;
  std::int64_t lastSalary = 0;
  std::size_t salaryCount = 0;
  std::int64_t firstSsa = 0;
  std::int64_t lastSsa = 0;
  std::size_t ssaCount = 0;
  std::int64_t lastRevenue = 0;
  std::size_t revenueCount = 0;

  // 2c section B: the day indices of government deposits (SSA +
  // disability) — the allowed post-claim payday anchors.
  std::set<std::uint32_t> govDays;
};

[[nodiscard]] std::string dateOf(pl::time::TimePoint tp) {
  const auto cd = pl::time::toCalendarDate(tp);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", cd.year, cd.month, cd.day);
  return buf;
}

[[nodiscard]] std::string dateOfEpoch(std::int64_t epoch) {
  return dateOf(pl::time::fromEpochSeconds(epoch));
}

[[nodiscard]] const char *seedName(PersonaType t) {
  switch (t) {
  case PersonaType::student:
    return "student";
  case PersonaType::retiree:
    return "retiree";
  case PersonaType::freelancer:
    return "freelancer";
  case PersonaType::smallBusiness:
    return "smallBusiness";
  case PersonaType::highNetWorth:
    return "highNetWorth";
  case PersonaType::salaried:
    return "salaried";
  }
  return "?";
}

// The date a seed persona's REVENUE plan stops emitting, mirroring the
// generation gate personaAt(monthStart) == plan.persona: students
// leave `student` at workStart, business owners leave `smallBusiness`
// at the close, workers leave their type at the claiming date; retiree
// and HNW seeds never leave theirs (perpetual revenue is correct —
// until death, gated separately).
[[nodiscard]] std::optional<pl::time::TimePoint>
revenueGateEnd(const tlx::Timeline &tl) {
  switch (tl.seed) {
  case PersonaType::student:
    return tl.workStart;
  case PersonaType::smallBusiness:
    return tl.businessEnd;
  case PersonaType::salaried:
  case PersonaType::freelancer:
    return tl.retirement;
  case PersonaType::retiree:
  case PersonaType::highNetWorth:
    return std::nullopt;
  }
  return std::nullopt;
}

// Pooled pre/post mean-ticket accumulator for the 2c spending gate.
struct SplitStat {
  double preSum = 0.0;
  std::size_t preN = 0;
  double postSum = 0.0;
  std::size_t postN = 0;

  void add(std::uint32_t day, std::uint32_t splitDay, double amount) {
    if (day + kSplitBufferDays < splitDay) {
      preSum += amount;
      ++preN;
    } else if (day >= splitDay + kSplitBufferDays) {
      postSum += amount;
      ++postN;
    }
  }

  [[nodiscard]] double ratio() const {
    if (preN == 0 || postN == 0) {
      return 0.0;
    }
    return (postSum / static_cast<double>(postN)) /
           (preSum / static_cast<double>(preN));
  }
};

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  WorldSpec spec;
  spec.seed = kSeed;
  spec.window.start =
      pl::time::makeTime(pl::time::CalendarDate{.year = 1991, .month = 1,
                                                .day = 1});
  spec.window.days = kDays;
  spec.population = kPopulation;
  const GateWorld world(pools, spec);

  const auto &personas = world.people.personas;
  const auto &ownership = world.holdings.accounts.ownership;
  const auto &registry = world.holdings.accounts.registry;
  const auto windowStart = spec.window.start;
  const auto windowEnd = spec.window.endExcl();
  const auto startEpoch = pl::time::toEpochSeconds(windowStart);

  check(personas.timelines.size() == static_cast<std::size_t>(kPopulation),
        "timeline carrier covers the population");
  check(personas.birthDates.size() == static_cast<std::size_t>(kPopulation),
        "birth-date carrier covers the population");

  std::unordered_map<pl::entity::Key, pl::entity::PersonId,
                     std::hash<pl::entity::Key>>
      primaryOf;
  std::unordered_map<pl::entity::Key, pl::entity::PersonId,
                     std::hash<pl::entity::Key>>
      businessOf;
  for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
    if (ownership.byPersonOffset[p - 1] == ownership.byPersonOffset[p]) {
      continue;
    }
    primaryOf.emplace(registry.records[ownership.primaryIndex(p)].id, p);
    businessOf.emplace(pl::synth::businessId(p), p);
  }

  const auto salaryTag = channels::tag(channels::Legit::salary);
  const auto ssaTag = channels::tag(channels::Government::socialSecurity);
  const auto disabilityTag = channels::tag(channels::Government::disability);

  std::vector<PersonRows> rows(static_cast<std::size_t>(kPopulation) + 1);

  // [diag] channel census + unattributed-row census over screened().
  std::map<unsigned, std::size_t> channelCounts;
  std::size_t totalRows = 0;
  std::size_t unattributed = 0;

  for (const auto &t : world.streams.screened()) {
    ++totalRows;
    ++channelCounts[static_cast<unsigned>(t.session.channel.value)];

    const bool isSalary = t.session.channel.value == salaryTag.value;
    const bool isSsa = t.session.channel.value == ssaTag.value;
    const bool isDisability = t.session.channel.value == disabilityTag.value;

    if (isSalary || isSsa) {
      const auto it = primaryOf.find(t.target);
      if (it == primaryOf.end()) {
        ++unattributed;
        continue;
      }
      auto &pr = rows[it->second];
      if (isSalary) {
        if (pr.salaryCount == 0 || t.timestamp < pr.firstSalary) {
          pr.firstSalary = t.timestamp;
        }
        if (t.timestamp > pr.lastSalary) {
          pr.lastSalary = t.timestamp;
        }
        ++pr.salaryCount;
      } else {
        if (pr.ssaCount == 0 || t.timestamp < pr.firstSsa) {
          pr.firstSsa = t.timestamp;
        }
        if (t.timestamp > pr.lastSsa) {
          pr.lastSsa = t.timestamp;
        }
        ++pr.ssaCount;
        pr.govDays.insert(
            static_cast<std::uint32_t>((t.timestamp - startEpoch) / 86'400));
      }
      continue;
    }

    if (isDisability) {
      const auto it = primaryOf.find(t.target);
      if (it != primaryOf.end()) {
        rows[it->second].govDays.insert(
            static_cast<std::uint32_t>((t.timestamp - startEpoch) / 86'400));
      }
      continue;
    }

    auto it = primaryOf.find(t.target);
    if (it == primaryOf.end()) {
      it = businessOf.find(t.target);
      if (it == businessOf.end()) {
        ++unattributed;
        continue;
      }
    }
    auto &pr = rows[it->second];
    if (t.timestamp > pr.lastRevenue) {
      pr.lastRevenue = t.timestamp;
    }
    ++pr.revenueCount;
  }

  std::printf("[diag] screened rows total %zu, unattributed %zu\n", totalRows,
              unattributed);
  for (const auto &[chan, count] : channelCounts) {
    std::printf("[diag] channel 0x%02x rows %zu\n", chan, count);
  }

  struct SeedStat {
    std::size_t people = 0;
    std::size_t paid = 0;
    std::size_t salaryRows = 0;
    std::size_t withRevenue = 0;
  };
  std::map<PersonaType, SeedStat> bySeed;
  for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
    const auto &tl = personas.timelines[p - 1];
    auto &st = bySeed[tl.seed];
    ++st.people;
    if (rows[p].salaryCount > 0) {
      ++st.paid;
      st.salaryRows += rows[p].salaryCount;
    }
    if (rows[p].revenueCount > 0) {
      ++st.withRevenue;
    }
  }
  for (const auto &[seed, st] : bySeed) {
    std::printf("[diag] seed %-13s people %3zu paid %3zu salaryRows %6zu "
                "withRevenue %3zu\n",
                seedName(seed), st.people, st.paid, st.salaryRows,
                st.withRevenue);
  }

  int shownNoPay = 0;
  int shownPay = 0;
  for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
    const auto &tl = personas.timelines[p - 1];
    if (tl.seed != PersonaType::salaried) {
      continue;
    }
    const bool paid = rows[p].salaryCount > 0;
    if (!paid && shownNoPay < 3) {
      std::printf("[diag] salaried p%-3u UNPAID  working=%s retirement=%s "
                  "death=%s\n",
                  static_cast<unsigned>(p), seedName(tl.working),
                  dateOf(tl.retirement).c_str(), dateOf(tl.death).c_str());
      ++shownNoPay;
    } else if (paid && shownPay < 3) {
      std::printf("[diag] salaried p%-3u PAID %3zu rows [%s .. %s] "
                  "retirement=%s death=%s\n",
                  static_cast<unsigned>(p), rows[p].salaryCount,
                  dateOfEpoch(rows[p].firstSalary).c_str(),
                  dateOfEpoch(rows[p].lastSalary).c_str(),
                  dateOf(tl.retirement).c_str(), dateOf(tl.death).c_str());
      ++shownPay;
    }
    if (shownNoPay >= 3 && shownPay >= 3) {
      break;
    }
  }

  // --- Per-person gates (2b + H3 income death stops) -----------------
  std::size_t totalSalaryRows = 0;
  std::size_t inWindowRetirees = 0;
  std::size_t inWindowRetireesWithSsa = 0;
  std::size_t fullArcs = 0;
  std::size_t studentOnsets = 0;
  std::size_t postCloseWorkers = 0;
  std::size_t inWindowDeaths = 0;
  std::size_t deadWithPriorIncome = 0;
  int shownViolators = 0;
  int shownDeaths = 0;

  for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
    const auto &tl = personas.timelines[p - 1];
    const auto &pr = rows[p];
    const auto pid = std::to_string(static_cast<unsigned>(p));
    totalSalaryRows += pr.salaryCount;

    if (tl.seed == PersonaType::retiree) {
      check(pr.salaryCount == 0,
            "seed retiree " + pid + " has zero paychecks, got " +
                std::to_string(pr.salaryCount));
    }

    if (pr.salaryCount > 0) {
      const auto graceEnd = pl::time::addDays(tl.retirement, 10);
      check(pl::time::fromEpochSeconds(pr.lastSalary) < graceEnd,
            "person " + pid + " paid after their claiming date");
    }

    if (tl.seed == PersonaType::student && pr.salaryCount > 0) {
      const auto onsetFloor = pl::time::addDays(tl.workStart, -2);
      check(pl::time::fromEpochSeconds(pr.firstSalary) >= onsetFloor,
            "student " + pid + " paid before the study->work transition");
      if (tl.workStart > windowStart && tl.workStart < windowEnd) {
        ++studentOnsets;
      }
    }

    if (pr.ssaCount > 0) {
      const auto onset = std::max(windowStart, tl.retirement);
      check(pl::time::fromEpochSeconds(pr.firstSsa) >=
                pl::time::addDays(onset, -1),
            "person " + pid + " drew Social Security before their claim");
    }

    if (pr.salaryCount > 0 && pr.ssaCount > 0) {
      check(pr.lastSalary <
                pl::time::toEpochSeconds(
                    pl::time::addDays(pl::time::fromEpochSeconds(pr.firstSsa),
                                      45)),
            "person " + pid + " paid long after Social Security began");
      ++fullArcs;
    }

    if (tl.seed != PersonaType::retiree && tl.retirement > windowStart &&
        tl.retirement < windowEnd) {
      ++inWindowRetirees;
      if (pr.ssaCount > 0) {
        ++inWindowRetireesWithSsa;
      }
    }

    // REVENUE STOP: mirror the generation gate exactly (month-start
    // granularity + settlement lag => 45d grace); retiree/HNW seeds
    // are exempt (their revenue persona never transitions away).
    if (pr.revenueCount > 0) {
      if (const auto gateEnd = revenueGateEnd(tl); gateEnd.has_value()) {
        const bool ok = pl::time::fromEpochSeconds(pr.lastRevenue) <
                        pl::time::addDays(*gateEnd, 45);
        if (!ok && shownViolators < 3) {
          std::printf("[diag] revenue violator p%-3u seed=%s working=%s "
                      "gateEnd=%s lastRevenue=%s revenueRows=%zu\n",
                      static_cast<unsigned>(p), seedName(tl.seed),
                      seedName(tl.working), dateOf(*gateEnd).c_str(),
                      dateOfEpoch(pr.lastRevenue).c_str(), pr.revenueCount);
          ++shownViolators;
        }
        check(ok, "person " + pid + " earned revenue past their persona gate");
      }
    }

    if (tl.seed == PersonaType::smallBusiness && tl.businessEnd < windowEnd &&
        tl.businessEnd < tl.retirement && pr.salaryCount > 0 &&
        pl::time::fromEpochSeconds(pr.lastSalary) > tl.businessEnd) {
      ++postCloseWorkers;
    }

    // --- H3: income stops at death --------------------------------
    check(tl.death > windowStart,
          "person " + pid + " satisfies the alive-at-start invariant");

    if (pr.salaryCount > 0) {
      check(pl::time::fromEpochSeconds(pr.lastSalary) <
                pl::time::addDays(tl.death, 10),
            "person " + pid + " was paid after their death");
    }
    if (pr.ssaCount > 0) {
      check(pl::time::fromEpochSeconds(pr.lastSsa) < tl.death,
            "person " + pid + " drew Social Security after their death");
    }
    if (pr.revenueCount > 0) {
      check(pl::time::fromEpochSeconds(pr.lastRevenue) <
                pl::time::addDays(tl.death, 45),
            "person " + pid + " earned revenue past their death month");
    }

    if (tl.death < windowEnd) {
      ++inWindowDeaths;
      if (pr.salaryCount > 0 || pr.ssaCount > 0 || pr.revenueCount > 0) {
        ++deadWithPriorIncome;
      }
      if (shownDeaths < 3) {
        std::printf("[diag] death p%-3u seed=%s death=%s salary=%zu ssa=%zu "
                    "revenue=%zu\n",
                    static_cast<unsigned>(p), seedName(tl.seed),
                    dateOf(tl.death).c_str(), pr.salaryCount, pr.ssaCount,
                    pr.revenueCount);
        ++shownDeaths;
      }
    }
  }

  check(totalSalaryRows > 5000,
        "salary rows populated (" + std::to_string(totalSalaryRows) + ")");
  check(inWindowRetirees >= 3, "in-window retirees exist (" +
                                   std::to_string(inWindowRetirees) + ")");
  check(inWindowRetireesWithSsa >= 1,
        "an in-window retiree draws Social Security (" +
            std::to_string(inWindowRetireesWithSsa) + " of " +
            std::to_string(inWindowRetirees) + ")");
  check(fullArcs >= 1,
        "a full worked->retired->deposited arc exists (" +
            std::to_string(fullArcs) + ")");
  check(studentOnsets >= 1,
        "a student starts a career in-window (" +
            std::to_string(studentOnsets) + ")");
  check(postCloseWorkers >= 1,
        "a small-business owner takes a job after the close (" +
            std::to_string(postCloseWorkers) + ")");
  std::printf("[diag] in-window deaths %zu (with prior income %zu)\n",
              inWindowDeaths, deadWithPriorIncome);
  check(inWindowDeaths >= 1,
        "in-window deaths exist (" + std::to_string(inWindowDeaths) + ")");
  check(deadWithPriorIncome >= 1,
        "a decedent had prior income — the stop is realized (" +
            std::to_string(deadWithPriorIncome) + ")");

  // --- 2c section A: end-of-window persona resolution -----------------
  // The seam both AML streaming sinks call from takeArtifacts(): the
  // Customer table must echo personaAt(corpus end) exactly.
  {
    const auto lastTs = pl::time::toEpochSeconds(windowEnd) - 1;
    const auto at = pl::time::fromEpochSeconds(lastTs);

    amlv::SharedContext ctx;
    ctx.personaByPerson = personas.assignment.byPerson;
    amlv::resolveEndOfWindowPersonas(ctx, personas, lastTs, /*rows=*/1);

    std::size_t changed = 0;
    for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
      const auto &tl = personas.timelines[p - 1];
      const auto expected = tlx::personaAt(tl, at);
      check(ctx.personaByPerson[p - 1] == expected,
            "end-of-window persona echoes personaAt for person " +
                std::to_string(static_cast<unsigned>(p)));
      if (tl.seed == PersonaType::retiree) {
        check(ctx.personaByPerson[p - 1] == PersonaType::retiree,
              "seed retiree stays retiree at window end");
      }
      if (tl.seed == PersonaType::highNetWorth) {
        check(ctx.personaByPerson[p - 1] == PersonaType::highNetWorth,
              "highNetWorth stays exempt at window end");
      }
      if (expected != tl.seed) {
        ++changed;
      }
    }
    std::printf("[diag] end-of-window personas changed %zu of %d\n", changed,
                kPopulation);
    check(changed >= inWindowRetirees,
          "the era echo covers at least the in-window retiree cohort (" +
              std::to_string(changed) + ")");

    // Empty stream / missing timeline lane leave the seed assignment.
    amlv::SharedContext ctxEmpty;
    ctxEmpty.personaByPerson = personas.assignment.byPerson;
    amlv::resolveEndOfWindowPersonas(ctxEmpty, personas, lastTs, /*rows=*/0);
    check(ctxEmpty.personaByPerson == personas.assignment.byPerson,
          "an empty stream leaves the seed assignment standing");

    amlv::SharedContext ctxShort;
    ctxShort.personaByPerson.assign(3, PersonaType::salaried);
    amlv::resolveEndOfWindowPersonas(ctxShort, personas, lastTs, /*rows=*/1);
    check(std::all_of(ctxShort.personaByPerson.begin(),
                      ctxShort.personaByPerson.end(),
                      [](PersonaType t) {
                        return t == PersonaType::salaried;
                      }),
          "a size-mismatched pack leaves the context untouched");
  }

  // --- 2c section B: payday re-anchor to SSA deposit days -------------
  // buildPaydaysByPerson screens by isPaydayInbound, which admits the
  // SSA channels — so a full-arc retiree's post-claim paydays ARE their
  // government deposit days. Persons with revenue are excluded (their
  // owner-draw/settlement inbounds are legitimate paydays too).
  {
    const auto paydaySets =
        pl::transfers::legit::blueprints::buildPaydaysByPerson(
            world.streams.screened(),
            {.registry = &registry,
             .lookup = &world.holdings.accounts.lookup},
            spec.window, static_cast<std::uint32_t>(kPopulation));

    std::size_t reanchored = 0;
    for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
      const auto &tl = personas.timelines[p - 1];
      const auto &pr = rows[p];
      if (pr.salaryCount == 0 || pr.ssaCount == 0 || pr.revenueCount > 0) {
        continue;
      }

      const auto claim = std::max(windowStart, tl.retirement);
      const auto graceEpoch =
          pl::time::toEpochSeconds(pl::time::addDays(claim, 45));
      const auto graceDay =
          static_cast<std::uint32_t>((graceEpoch - startEpoch) / 86'400);

      bool anyPost = false;
      for (const auto day : paydaySets[p - 1]) {
        if (day < graceDay) {
          continue;
        }
        anyPost = true;
        check(pr.govDays.count(day) != 0,
              "person " + std::to_string(static_cast<unsigned>(p)) +
                  " has a post-claim payday that is not a government "
                  "deposit day (day " +
                  std::to_string(day) + ")");
      }
      if (anyPost) {
        ++reanchored;
      }
    }
    std::printf("[diag] payday re-anchor validated on %zu retirees\n",
                reanchored);
    check(reanchored >= 1,
          "at least one retiree's payday set re-anchors to SSA days (" +
              std::to_string(reanchored) + ")");
  }

  // --- 2c section C + H3 spending stop ---------------------------------
  // A real spending run (base routines + simulator). Gates: (i) the
  // in-window retiree cohort's mean ticket drops relative to a
  // non-retiring salaried control (difference-in-differences around
  // each claiming day; generous band — the 0.88 level factor compounds
  // with the SSA-income liquidity response); (ii) H3 — ZERO spending
  // rows sourced from a dead spender on or after their death day, and
  // >=1 in-window-dead spender spent while alive.
  {
    WorldSpec spec2;
    spec2.seed = kSeed;
    spec2.window.start = spec.window.start;
    spec2.window.days = kSpendDays;
    spec2.population = kPopulation;
    spec2.withBaseRoutines = true;
    GateWorld world2(pools, spec2);

    namespace routineSpending = pl::transfers::legit::routines::spending;
    const routineSpending::SpendingRoutine routine;
    const auto spendRows = routine.run(
        routineSpending::SpendingRoutine::Execution{
            .rng = world2.rng, .txf = *world2.txf, .seed = kSeed},
        world2.market, world2.obligations, world2.screenBook);

    // Source-account attribution: primary deposit accounts + cards.
    std::unordered_map<pl::entity::Key, pl::entity::PersonId,
                       std::hash<pl::entity::Key>>
        spenderOf;
    const auto &ownership2 = world2.holdings.accounts.ownership;
    const auto &registry2 = world2.holdings.accounts.registry;
    for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
      if (ownership2.byPersonOffset[p - 1] == ownership2.byPersonOffset[p]) {
        continue;
      }
      spenderOf.emplace(registry2.records[ownership2.primaryIndex(p)].id, p);
    }
    for (const auto &rec : world2.holdings.creditCards.records) {
      if (rec.owner != pl::entity::invalidPerson && rec.owner >= 1 &&
          rec.owner <= static_cast<pl::entity::PersonId>(kPopulation)) {
        spenderOf.emplace(rec.key, rec.owner);
      }
    }

    // Cohorts from the (identically derived) world2 timelines.
    const auto &timelines2 = world2.people.personas.timelines;
    const auto start2Epoch = pl::time::toEpochSeconds(spec2.window.start);
    const auto end2Epoch =
        start2Epoch + static_cast<std::int64_t>(kSpendDays) * 86'400;

    constexpr std::uint32_t kNoDay = 0xFFFF'FFFFu;
    std::vector<std::uint32_t> claimDay(
        static_cast<std::size_t>(kPopulation) + 1, kNoDay);
    std::vector<std::uint32_t> deathDay(
        static_cast<std::size_t>(kPopulation) + 1, kNoDay);
    std::vector<bool> isControl(static_cast<std::size_t>(kPopulation) + 1,
                                false);
    std::vector<bool> spentAlive(static_cast<std::size_t>(kPopulation) + 1,
                                 false);
    std::vector<std::uint32_t> treatedDays;
    for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
      const auto &tl = timelines2[p - 1];

      const auto death = pl::time::toEpochSeconds(tl.death);
      if (death < end2Epoch) {
        deathDay[p] = static_cast<std::uint32_t>(
            std::max<std::int64_t>(0, (death - start2Epoch) / 86'400));
      }

      if (tl.seed == PersonaType::retiree ||
          tl.seed == PersonaType::highNetWorth) {
        continue;
      }
      const auto claim = pl::time::toEpochSeconds(tl.retirement);
      if (claim >= end2Epoch) {
        if (tl.seed == PersonaType::salaried) {
          isControl[p] = true;
        }
        continue;
      }
      const auto day = static_cast<std::uint32_t>(
          std::max<std::int64_t>(0, (claim - start2Epoch) / 86'400));
      if (day >= kCohortMarginDays &&
          day + kCohortMarginDays < static_cast<std::uint32_t>(kSpendDays)) {
        claimDay[p] = day;
        treatedDays.push_back(day);
      }
    }

    check(!treatedDays.empty(),
          "an interior in-window retiree cohort exists in the spending leg");

    std::sort(treatedDays.begin(), treatedDays.end());
    const std::uint32_t medianClaimDay =
        treatedDays.empty() ? 0 : treatedDays[treatedDays.size() / 2];

    const auto merchantTag = channels::tag(channels::Legit::merchant);
    const auto cardTag = channels::tag(channels::Legit::cardPurchase);
    const auto billTag = channels::tag(channels::Legit::bill);
    const auto p2pTag = channels::tag(channels::Legit::p2p);
    const auto externalTag = channels::tag(channels::Legit::externalUnknown);

    SplitStat treated;
    SplitStat control;
    std::size_t deadSpendViolations = 0;
    for (const auto &t : spendRows) {
      const auto ch = t.session.channel.value;
      if (ch != merchantTag.value && ch != cardTag.value &&
          ch != billTag.value && ch != p2pTag.value &&
          ch != externalTag.value) {
        continue;
      }
      const auto it = spenderOf.find(t.source);
      if (it == spenderOf.end()) {
        continue;
      }
      const auto p = it->second;
      const auto day = static_cast<std::uint32_t>(
          std::max<std::int64_t>(0, (t.timestamp - start2Epoch) / 86'400));

      // H3: the emission loop stops a dead spender's person-days at
      // their death day — strictly no row on or after it.
      if (deathDay[p] != kNoDay) {
        if (day >= deathDay[p]) {
          ++deadSpendViolations;
        } else {
          spentAlive[p] = true;
        }
      }

      if (claimDay[p] != kNoDay) {
        treated.add(day, claimDay[p], t.amount);
      } else if (isControl[p]) {
        control.add(day, medianClaimDay, t.amount);
      }
    }

    std::size_t deadSpenders = 0;
    std::size_t deadWhoSpentAlive = 0;
    for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
      if (deathDay[p] != kNoDay) {
        ++deadSpenders;
        if (spentAlive[p]) {
          ++deadWhoSpentAlive;
        }
      }
    }
    std::printf("[diag] spending-leg deaths %zu (spent while alive %zu), "
                "post-death spend violations %zu\n",
                deadSpenders, deadWhoSpentAlive, deadSpendViolations);
    check(deadSpendViolations == 0,
          "no spending row is sourced from a dead spender (" +
              std::to_string(deadSpendViolations) + " violations)");
    check(deadSpenders >= 1, "the spending leg carries in-window deaths (" +
                                 std::to_string(deadSpenders) + ")");
    check(deadWhoSpentAlive >= 1,
          "a decedent spent while alive — the stop is realized (" +
              std::to_string(deadWhoSpentAlive) + ")");

    std::printf("[diag] spending rows %zu; treated persons %zu "
                "(pre %zu / post %zu tickets), control pre %zu / post %zu\n",
                spendRows.size(), treatedDays.size(), treated.preN,
                treated.postN, control.preN, control.postN);

    check(treated.preN >= 100 && treated.postN >= 100,
          "the treated cohort has ticket mass on both sides of the claim");
    check(control.preN >= 100 && control.postN >= 100,
          "the control cohort has ticket mass on both sides of the split");
    check(treated.postN > 0,
          "retired spenders keep spending after the claim (SSA-funded)");

    const double rTreated = treated.ratio();
    const double rControl = control.ratio();
    const double did = rControl > 0.0 ? rTreated / rControl : 0.0;
    std::printf("[diag] mean-ticket post/pre: treated %.4f control %.4f "
                "difference-in-differences %.4f (step factor %.2f)\n",
                rTreated, rControl, did,
                pl::activity::spending::actors::kRetiredSpendScale);

    check(did > 0.0 && did < 0.97,
          "the retiree cohort's tickets step DOWN relative to control (" +
              std::to_string(did) + ")");
    check(did > 0.60,
          "the retirement step is a step, not a collapse (" +
              std::to_string(did) + ")");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d gate(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_persona_wiring: all gates passed (retirees in-window %zu, "
              "with SSA %zu, full arcs %zu, student onsets %zu, post-close "
              "workers %zu, salary rows %zu, in-window deaths %zu)\n",
              inWindowRetirees, inWindowRetireesWithSsa, fullArcs,
              studentOnsets, postCloseWorkers, totalSalaryRows,
              inWindowDeaths);
  return 0;
}
