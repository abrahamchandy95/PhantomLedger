//
// tests/test_tuition_payees.cpp
//
// tuition-payee-2026-09: ONE SCHOOL PER STUDENT, NOT ONE PER RUN.
//
// `tuition::generate` drew its payee once per run, before the student loop,
// so every tuition installment in the population paid one education-category
// catalogue merchant (32,398 rows on one external account at pop 200,000 over
// 2024), whether or not that merchant was still open. Tuition is a school
// payment plan (the owner's decision, recorded in the
// `# AMENDMENT: tuition-payee-2026-09` section of docs/fraud_model_audit.md),
// so each paying student now pays a school of their own: an education record
// in their home area when one is open for the whole plan, else any open
// education record, drawn on the student's own {"family", "tuition-school",
// student} lane. The retired per-run draw is still spent on the
// {"family", "tuition"} lane, so nothing else moves.
//
//   A  LANE ISOLATION, on the run-golden world (pop 2,000, 60 days from
//      2025-01-01, seed 3405691582, income off). Pins measured on the
//      pre-round build:
//      A1 the shared entity stream after the build, read after the family
//         pass has run;
//      A2 an order-free digest of every family row that is not tuition,
//         every field including the device and IP;
//      A3 the tuition rows with the target masked, and their count;
//      A4 DISARM: the same pass with tuition switched off moves A2, because
//         every family routine routes device and IP on one shared lane, so
//         A2 can fail if tuition emits a different number of rows;
//      A5 the tuition targets did move off the pre-round build;
//      A6 with every school closed, no tuition row is paid and A2 still
//         holds: a plan with no open school is made and dropped, so the
//         shared routing lane does not see it.
//
//   B  THE PAYEE DOMAIN, on the family pass: the run-golden world and a
//      scale leg at the mule-temporal corpus's configuration (pop 200,000,
//      2024, seed 42, income and products off).
//      B1 every tuition target is an education catalogue record open at the
//         row's date;
//      B2 no account takes every tuition row, and the largest school's share
//         is bounded (the retired law scored 1.0);
//      B3 attributed to its student through the family graph, each plan pays
//         one school, in the student's home area when that area had one open
//         for the whole plan; the leg must carry enough plans with a local
//         option that a national pick would break the check.
//
//   C  THE PURE LAW, on a hand-built catalogue: home area first, the
//      national fallback, liveness over the whole plan, one draw or none,
//      and the burn's range.
//
//   D  THE SETTLED CORPUS at the run-golden configuration, through the whole
//      windowed engine: the domain predicate paired with the re-pinned
//      tests/golden_run.b2sum. Tuition rows are present, every one pays an
//      education record open at its date, no account takes all of them and
//      the largest school stays under the anti-hub bound.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/synth/counterparties/remote_payees.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"
#include "phantomledger/transfers/legit/routines/family/schools.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include "gate_world.hpp"
#include "window_leg_support.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;
namespace relatives = pl::transfers::legit::routines::relatives;
namespace family_rt = pl::transfers::legit::routines::family;
namespace family_rel = pl::relationships::family;
namespace schools = pl::transfers::legit::routines::family::schools;
namespace geo = pl::entity::geography;
namespace remote = pl::synth::counterparties::remote;
using Category = pl::merchants::Category;

using Txn = pl::transactions::Transaction;
using Key = pl::entity::Key;

namespace {

constexpr std::uint64_t kGoldenSeed = 3405691582ULL;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

const auto kTuitionTag = channels::tag(channels::Family::tuition).value;

[[nodiscard]] bool isTuition(const Txn &row) {
  return row.session.channel.value == kTuitionTag;
}

[[nodiscard]] std::uint64_t splitmix(std::uint64_t value) {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

[[nodiscard]] std::uint64_t keyWord(Key key) {
  return splitmix(key.number ^ (static_cast<std::uint64_t>(key.role) << 56U) ^
                  (static_cast<std::uint64_t>(key.bank) << 48U));
}

[[nodiscard]] std::uint64_t rowHash(const Txn &row, bool maskTarget) {
  std::uint64_t h = splitmix(keyWord(row.source));
  h = splitmix(h ^ (maskTarget ? 0x5A5A5A5AULL : keyWord(row.target)));
  h = splitmix(h ^ std::bit_cast<std::uint64_t>(row.amount));
  h = splitmix(h ^ static_cast<std::uint64_t>(row.timestamp));
  h = splitmix(h ^ row.session.channel.value);
  h = splitmix(h ^ row.fraud.flag);
  h = splitmix(h ^ static_cast<std::uint64_t>(row.fraud.type));
  const auto &device = row.session.deviceId;
  h = splitmix(h ^ static_cast<std::uint64_t>(device.ownerType));
  h = splitmix(h ^ device.ownerId);
  h = splitmix(h ^ device.slot);
  h = splitmix(h ^ row.session.ipAddress.value);
  return h;
}

struct FamilyDigest {
  std::size_t otherRows = 0;
  std::uint64_t other = 0;
  std::size_t tuitionRows = 0;
  std::uint64_t tuitionMasked = 0;
  std::uint64_t tuitionFull = 0;
};

[[nodiscard]] FamilyDigest digestOf(const std::vector<Txn> &rows) {
  FamilyDigest d;
  for (const auto &row : rows) {
    if (isTuition(row)) {
      ++d.tuitionRows;
      d.tuitionMasked += rowHash(row, true);
      d.tuitionFull += rowHash(row, false);
    } else {
      ++d.otherRows;
      d.other += rowHash(row, false);
    }
  }
  return d;
}

// The production family composition (builder.cpp, window_leg_support.hpp).
// Each pass routes on its OWN copy of the world's pristine family router, as
// production does: `make` advances the router's sticky device and IP state,
// so a second pass on the same router would start where the first stopped.
[[nodiscard]] std::vector<Txn>
runFamily(const pltest::GateWorld &world, std::uint64_t seed,
          const relatives::FamilyTransferModel &model,
          const pl::entity::merchant::Catalog *catalog = nullptr) {
  relatives::FamilyTransferScenario scenario;
  scenario.households(family_rel::kDefaultHouseholds)
      .dependents(family_rel::kDefaultDependents)
      .retireeSupport(family_rel::kDefaultRetireeSupport)
      .transfers(model);

  const relatives::FamilyLedgerSources sources{
      .accounts = &world.holdings.accounts.registry,
      .ownership = &world.holdings.accounts.ownership,
      .educationMerchants = catalog != nullptr ? catalog : &world.cps.merchants,
  };

  const pl::infra::Router router = world.familyRouter;
  const pl::random::RngFactory familyRngFactory{seed};
  auto familyRoutingRng = familyRngFactory.rng({"family", "routing"});
  const pl::transactions::Factory familyTxf(familyRoutingRng, &router);
  return relatives::generateFamilyTxns(
      world.plan, sources,
      family_rt::TransferEmission{familyRngFactory, familyTxf}, scenario);
}

// ------------------------------------------------------------------ A

// Measured on the pre-round build (the staged hub-realism tree, exported
// with `git checkout-index` and built separately) and on this build.
constexpr std::uint64_t kSharedStreamNext = 0xddfd735e74a97cd2ULL;
constexpr std::size_t kOtherFamilyRows = 2'613;
constexpr std::uint64_t kOtherFamilyDigest = 0x3380fc1f52318eb5ULL;
constexpr std::size_t kTuitionRows = 210;
constexpr std::uint64_t kTuitionMaskedDigest = 0xc748e4816d8e5467ULL;
// The pre-round build's tuition digest with the target in it. A5 requires
// this build to differ: every row paid one account there.
constexpr std::uint64_t kPreRoundTuitionFullDigest = 0x496b31f798be1121ULL;

void gateA(const pl::synth::pii::PoolSet &poolSet) {
  std::printf("\n##### A: lane isolation on the run-golden world #####\n");

  pltest::WorldSpec spec;
  spec.seed = kGoldenSeed;
  spec.window =
      pl::time::Window{.start = pl::time::makeTime({2025, 1, 1}), .days = 60};
  spec.population = 2'000;
  spec.fraudProfile = pltest::scaledFraudProfile();
  spec.withIncome = false;
  const pltest::GateWorld world(poolSet, spec);

  const auto rows =
      runFamily(world, kGoldenSeed, relatives::kDefaultFamilyTransferModel);
  const auto armed = digestOf(rows);

  // A1, read AFTER the family pass: it holds no handle on the shared stream,
  // and this proves it.
  auto shared = world.rng;
  const auto next = shared.nextU64();
  auto fresh = pl::random::Rng::fromSeed(kGoldenSeed);
  std::printf("  A1 shared stream next %016llx (pinned %016llx)\n",
              static_cast<unsigned long long>(next),
              static_cast<unsigned long long>(kSharedStreamNext));
  check(next == kSharedStreamNext,
        "A1: the shared entity stream moved off the pre-round build");
  check(next != fresh.nextU64(),
        "A1: the build drew nothing from the shared stream, so the pin "
        "would pass on no data");

  std::map<std::uint8_t, std::size_t> byChannel;
  for (const auto &row : rows) {
    ++byChannel[row.session.channel.value];
  }
  std::printf("  family rows by channel:");
  for (const auto &[tag, n] : byChannel) {
    std::printf(" 0x%02x=%zu", static_cast<unsigned>(tag), n);
  }
  std::printf("\n");

  std::printf("  A2 other family rows %zu (pinned %zu), digest %016llx "
              "(pinned %016llx)\n",
              armed.otherRows, kOtherFamilyRows,
              static_cast<unsigned long long>(armed.other),
              static_cast<unsigned long long>(kOtherFamilyDigest));
  check(armed.otherRows == kOtherFamilyRows,
        "A2: the non-tuition family row count moved");
  check(armed.other == kOtherFamilyDigest,
        "A2: a non-tuition family row moved (any field, device and IP "
        "included)");

  std::printf("  A3 tuition rows %zu (pinned %zu), target-masked digest "
              "%016llx (pinned %016llx)\n",
              armed.tuitionRows, kTuitionRows,
              static_cast<unsigned long long>(armed.tuitionMasked),
              static_cast<unsigned long long>(kTuitionMaskedDigest));
  check(armed.tuitionRows == kTuitionRows, "A3: the tuition row count moved");
  check(armed.tuitionMasked == kTuitionMaskedDigest,
        "A3: a tuition row moved in a field other than its target");

  auto disarmedModel = relatives::kDefaultFamilyTransferModel;
  disarmedModel.tuition.enabled = false;
  const auto disarmed = digestOf(runFamily(world, kGoldenSeed, disarmedModel));
  std::printf("  A4 disarm (tuition off): other family rows %zu, digest "
              "%016llx (%s)\n",
              disarmed.otherRows,
              static_cast<unsigned long long>(disarmed.other),
              disarmed.other != kOtherFamilyDigest ? "red" : "GREEN");
  check(armed.tuitionRows > 0,
        "A4: the world has no tuition row, so nothing above is measured");
  check(disarmed.other != kOtherFamilyDigest,
        "A4: removing every tuition row left the other family rows "
        "unmoved, so A2 cannot see a tuition row-count change");

  std::printf("  A5 tuition digest with targets %016llx (pre-round %016llx)\n",
              static_cast<unsigned long long>(armed.tuitionFull),
              static_cast<unsigned long long>(kPreRoundTuitionFullDigest));
  check(armed.tuitionFull != kPreRoundTuitionFullDigest,
        "A5: the tuition targets are the pre-round build's");

  // A6: every school closed (each education record's operating interval
  // emptied). Each plan then has no payee and its rows are dropped, but they
  // must still be MADE, or every family row routed after them moves.
  auto shut = world.cps.merchants;
  for (auto &record : shut.records) {
    if (record.category == Category::education) {
      record.lastEpochExcl = record.firstEpoch;
    }
  }
  const auto closed = digestOf(runFamily(
      world, kGoldenSeed, relatives::kDefaultFamilyTransferModel, &shut));
  std::printf("  A6 every school closed: tuition rows %zu, other family rows "
              "%zu, digest %016llx\n",
              closed.tuitionRows, closed.otherRows,
              static_cast<unsigned long long>(closed.other));
  check(closed.tuitionRows == 0,
        "A6: a plan with no open school still paid one");
  check(closed.otherRows == kOtherFamilyRows &&
            closed.other == kOtherFamilyDigest,
        "A6: dropping the plans with no open school moved the other family "
        "rows, so their rows were not made");
}

// ------------------------------------------------------------------ B

// The anti-hub bound on the largest school's share of tuition rows. The
// retired law scored 1.0 by construction. Under the home-area law a school's
// share is about its area's share of paying students divided by the area's
// open schools, plus its slice of the national fallback, so the bound must
// clear the busiest area's share, which a single open school there would
// take (printed as the reference: 0.1758 of the attributed plans at pop 2,000
// and 0.1631 at pop 200,000). Measured: 0.0667 on the run-golden world and
// 0.0042 at pop 200,000.
constexpr double kMaxSchoolShare = 0.25;

struct PlanSpan {
  Key school{};
  std::int64_t first = std::numeric_limits<std::int64_t>::max();
  std::int64_t last = std::numeric_limits<std::int64_t>::min();
  std::size_t rows = 0;
};

void gateB(const pl::synth::pii::PoolSet &poolSet, const char *label,
           const pltest::WorldSpec &spec, double maxShareBound,
           std::size_t minLocalOptionPlans, double minBlindClosed) {
  std::printf("\n##### B: the payee domain, %s #####\n", label);
  const pltest::GateWorld world(poolSet, spec);
  const auto rows =
      runFamily(world, spec.seed, relatives::kDefaultFamilyTransferModel);

  const auto &catalog = world.cps.merchants;
  std::unordered_map<Key, std::size_t, std::hash<Key>> recordOf;
  std::size_t educationRecords = 0;
  for (std::size_t i = 0; i < catalog.records.size(); ++i) {
    if (catalog.records[i].category == Category::education) {
      recordOf.emplace(catalog.records[i].counterpartyId, i);
      ++educationRecords;
    }
  }

  // B1 and B2.
  std::size_t tuitionRows = 0;
  std::size_t offDomain = 0;
  std::size_t closedAtRow = 0;
  std::int64_t spanFirst = std::numeric_limits<std::int64_t>::max();
  std::int64_t spanLast = std::numeric_limits<std::int64_t>::min();
  std::unordered_map<Key, std::size_t, std::hash<Key>> rowsBySchool;
  for (const auto &row : rows) {
    if (!isTuition(row)) {
      continue;
    }
    ++tuitionRows;
    spanFirst = std::min(spanFirst, row.timestamp);
    spanLast = std::max(spanLast, row.timestamp);
    ++rowsBySchool[row.target];
    const auto it = recordOf.find(row.target);
    if (it == recordOf.end()) {
      ++offDomain;
      continue;
    }
    if (!catalog.records[it->second].liveAt(row.timestamp)) {
      ++closedAtRow;
    }
  }

  // Education records that open or close inside the tuition span: without
  // any, B1's liveness half is not exercised on this leg (C carries it).
  std::size_t turnover = 0;
  for (const auto &[key, idx] : recordOf) {
    const auto &record = catalog.records[idx];
    if ((record.firstEpoch > spanFirst && record.firstEpoch <= spanLast) ||
        (record.lastEpochExcl > spanFirst &&
         record.lastEpochExcl <= spanLast)) {
      ++turnover;
    }
  }

  std::size_t largest = 0;
  for (const auto &[key, n] : rowsBySchool) {
    largest = std::max(largest, n);
  }
  const double largestShare =
      tuitionRows == 0
          ? 1.0
          : static_cast<double>(largest) / static_cast<double>(tuitionRows);

  std::printf("  %zu tuition rows over %zu schools (%zu education records, "
              "%zu opening or closing inside the tuition span)\n",
              tuitionRows, rowsBySchool.size(), educationRecords, turnover);
  std::printf("  B1 off the education catalogue %zu, closed at the row %zu\n",
              offDomain, closedAtRow);
  std::printf("  B2 largest school %zu rows, share %.4f (bound %.2f; the "
              "retired law 1.0)\n",
              largest, largestShare, maxShareBound);

  check(tuitionRows > 0, std::string{"B: "} + label + " has no tuition row");
  check(offDomain == 0, std::string{"B1: "} + label +
                            ": a tuition row pays an account that is not an "
                            "education catalogue record");
  check(closedAtRow == 0, std::string{"B1: "} + label +
                              ": a tuition row pays a school closed at its "
                              "date");
  check(rowsBySchool.size() >= 2 && largestShare < 1.0,
        std::string{"B2: "} + label + ": one account takes every tuition row");
  check(largestShare <= maxShareBound, std::string{"B2: "} + label +
                                           ": the largest school's share of "
                                           "tuition rows is above the bound");

  // B3: attribute each plan to its student. A parent's local account can
  // pay for several students; only plans whose payer has exactly one
  // student child are attributed, and the rest are counted.
  const auto graph = relatives::buildFamilyGraph(
      world.plan, family_rel::kDefaultHouseholds,
      family_rel::kDefaultDependents, family_rel::kDefaultRetireeSupport);
  const auto personas = relatives::personasView(world.plan);
  const family_rt::FamilyAccountDirectory directory{
      world.holdings.accounts.registry, world.holdings.accounts.ownership,
      family_rt::kDefaultCounterpartyRouting};

  std::unordered_map<Key, std::vector<pl::entity::PersonId>, std::hash<Key>>
      studentsOfPayer;
  for (std::size_t i = 0; i < personas.size(); ++i) {
    if (!pl::relationships::family::predicates::isStudent(personas[i])) {
      continue;
    }
    const auto student = static_cast<pl::entity::PersonId>(i + 1);
    std::set<Key> payers;
    for (const auto parent : graph.parentsFor(student)) {
      if (!pl::entity::valid(parent)) {
        continue;
      }
      if (const auto acct = directory.localMemberAccount(parent)) {
        payers.insert(*acct);
      }
    }
    for (const auto &acct : payers) {
      studentsOfPayer[acct].push_back(student);
    }
  }

  std::unordered_map<Key, std::vector<PlanSpan>, std::hash<Key>> plansOfPayer;
  for (const auto &row : rows) {
    if (!isTuition(row)) {
      continue;
    }
    auto &plans = plansOfPayer[row.source];
    auto it = std::ranges::find_if(
        plans, [&](const PlanSpan &p) { return p.school == row.target; });
    if (it == plans.end()) {
      plans.push_back(PlanSpan{.school = row.target});
      it = std::prev(plans.end());
    }
    it->first = std::min(it->first, row.timestamp);
    it->last = std::max(it->last, row.timestamp);
    ++it->rows;
  }

  const auto &homeAreas = world.plan.counterparties().homeAreas;
  const auto *relocation = world.plan.counterparties().relocation;
  const auto openThroughout = [&](std::size_t idx, std::int64_t first,
                                  std::int64_t last) {
    return catalog.records[idx].liveAt(first) &&
           catalog.records[idx].liveAt(last);
  };

  std::size_t attributed = 0;
  std::size_t unattributed = 0;
  std::size_t splitPlans = 0;
  std::size_t inArea = 0;
  std::size_t fallback = 0;
  std::size_t misplaced = 0;
  std::size_t localOption = 0;
  double nationalMisplacedExpect = 0.0;
  // What a liveness-blind law (home area first, over every education record
  // there, else over all of them) would pay to a school not open for the
  // whole plan, in expectation.
  double blindClosedExpect = 0.0;
  std::map<geo::GeoAreaId, std::size_t> plansByArea;
  for (const auto &[payer, plans] : plansOfPayer) {
    const auto students = studentsOfPayer.find(payer);
    if (students == studentsOfPayer.end() || students->second.size() != 1) {
      ++unattributed;
      continue;
    }
    ++attributed;
    if (plans.size() != 1) {
      ++splitPlans;
      continue;
    }
    const auto &plan = plans.front();
    const auto student = students->second.front();
    const auto home =
        remote::homeAreaAt(homeAreas, relocation, student, plan.first);
    ++plansByArea[home];
    std::size_t localOpen = 0;
    std::size_t nationalOpen = 0;
    std::size_t localAll = 0;
    for (const auto &[key, idx] : recordOf) {
      const bool here =
          geo::validArea(home) && catalog.records[idx].location == home;
      localAll += here ? 1U : 0U;
      if (!openThroughout(idx, plan.first, plan.last)) {
        continue;
      }
      ++nationalOpen;
      localOpen += here ? 1U : 0U;
    }
    blindClosedExpect += localAll > 0
                             ? 1.0 - static_cast<double>(localOpen) /
                                         static_cast<double>(localAll)
                             : 1.0 - static_cast<double>(nationalOpen) /
                                         static_cast<double>(educationRecords);
    const auto school = recordOf.find(plan.school);
    if (school == recordOf.end()) {
      ++misplaced;
      continue;
    }
    const bool local = geo::validArea(home) &&
                       catalog.records[school->second].location == home;
    if (localOpen > 0) {
      ++localOption;
      nationalMisplacedExpect +=
          1.0 - static_cast<double>(localOpen) /
                    static_cast<double>(std::max<std::size_t>(1, nationalOpen));
      local ? ++inArea : ++misplaced;
    } else {
      ++fallback;
    }
  }

  std::printf("  B3 %zu payers attributed to one student (%zu paying for "
              "several, not attributed); %zu plans in the home area, %zu on "
              "the national fallback, %zu misplaced, %zu split over schools\n",
              attributed, unattributed, inArea, fallback, misplaced,
              splitPlans);
  std::size_t largestAreaPlans = 0;
  for (const auto &[area, n] : plansByArea) {
    largestAreaPlans = std::max(largestAreaPlans, n);
  }
  std::printf("  B2 reference: the busiest home area holds %.4f of the "
              "attributed plans\n",
              attributed == 0 ? 0.0
                              : static_cast<double>(largestAreaPlans) /
                                    static_cast<double>(attributed));
  std::printf("  B3 %zu plans had a local school open throughout; a national "
              "pick would misplace %.1f of them in expectation (floor %zu "
              "plans)\n",
              localOption, nationalMisplacedExpect, minLocalOptionPlans);
  check(splitPlans == 0, std::string{"B3: "} + label +
                             ": a student's plan pays more than one school");
  check(misplaced == 0, std::string{"B3: "} + label +
                            ": a plan skipped an open school in the "
                            "student's home area");
  check(localOption >= minLocalOptionPlans,
        std::string{"B3: "} + label +
            ": too few plans had a local option for the check to see a "
            "national pick");
  check(nationalMisplacedExpect >= 10.0,
        std::string{"B3: "} + label +
            ": a national pick would misplace fewer than 10 plans in "
            "expectation, so the check cannot fail");
  std::printf("  B1 a liveness-blind pick would pay a school closed for part "
              "of the plan on %.1f attributed plans in expectation\n",
              blindClosedExpect);
  if (minBlindClosed > 0.0) {
    check(blindClosedExpect >= minBlindClosed,
          std::string{"B1: "} + label +
              ": too little school turnover inside the plans for the "
              "liveness check to see a liveness-blind pick");
  }
}

// ------------------------------------------------------------------ C

[[nodiscard]] pl::entity::merchant::Record
educationRecord(std::uint64_t serial, geo::GeoAreaId area, std::int64_t open,
                std::int64_t closeExcl,
                Category category = Category::education) {
  pl::entity::merchant::Record r{};
  r.label = pl::entity::merchant::Label{serial};
  r.counterpartyId = pl::entity::makeKey(pl::entity::Role::merchant,
                                         pl::entity::Bank::external, serial);
  r.category = category;
  r.location = area;
  r.firstEpoch = open;
  r.lastEpochExcl = closeExcl;
  return r;
}

[[nodiscard]] bool sameState(pl::random::Rng a, pl::random::Rng b) {
  return a.nextU64() == b.nextU64() && a.nextU64() == b.nextU64();
}

void gateC() {
  std::printf("\n##### C: the pure law on a hand-built catalogue #####\n");
  constexpr std::int64_t kFirst = 1'000'000;
  constexpr std::int64_t kLast = 2'000'000;
  constexpr std::int64_t kAlways = std::numeric_limits<std::int64_t>::max();
  constexpr std::int64_t kNever = std::numeric_limits<std::int64_t>::min();
  constexpr geo::GeoAreaId kHome = 5;
  constexpr geo::GeoAreaId kClosingArea = 7;
  constexpr geo::GeoAreaId kOpeningArea = 9;

  pl::entity::merchant::Catalog catalog;
  catalog.records = {
      educationRecord(1, kHome, kNever, kAlways), // open
      educationRecord(2, kHome, kNever, kAlways), // open
      educationRecord(3, kHome, kNever, kFirst),  // closed
      educationRecord(4, kHome, kNever, kAlways, Category::grocery),
      educationRecord(5, kClosingArea, kNever, kLast), // closes mid-plan
      educationRecord(6, geo::invalidGeoArea, kNever, kAlways), // online
      educationRecord(7, kOpeningArea, kFirst + 1, kAlways), // opens mid-plan
  };
  const schools::Directory directory{catalog};

  std::printf("  C0 directory size %zu (6 education records, one grocery)\n",
              directory.size());
  check(directory.size() == 6,
        "C0: the burn's range must count every education record, open or "
        "not, and nothing else");

  const auto tally = [&](geo::GeoAreaId home) {
    std::map<std::uint64_t, std::size_t> hits;
    for (std::uint64_t lane = 0; lane < 4'000; ++lane) {
      auto rng = pl::random::Rng::fromSeed(0x7C00'0000ULL + lane);
      if (const auto key = directory.pick(home, kFirst, kLast, rng)) {
        ++hits[key->number];
      } else {
        ++hits[0];
      }
    }
    return hits;
  };
  const auto print = [](const char *what,
                        const std::map<std::uint64_t, std::size_t> &hits) {
    std::printf("  %s:", what);
    for (const auto &[serial, n] : hits) {
      std::printf(" %llu=%zu", static_cast<unsigned long long>(serial), n);
    }
    std::printf("\n");
  };
  // Uniform over k options, 4,000 picks: each within 5 sigma of 4000/k.
  const auto uniform = [](const std::map<std::uint64_t, std::size_t> &hits,
                          std::size_t k) {
    const double p = 1.0 / static_cast<double>(k);
    const double sigma = std::sqrt(4'000.0 * p * (1.0 - p));
    return std::ranges::all_of(hits, [&](const auto &kv) {
      return std::abs(static_cast<double>(kv.second) - 4'000.0 * p) <=
             5.0 * sigma;
    });
  };

  const auto home = tally(kHome);
  print("C1 home area 5", home);
  check(home.size() == 2 && home.contains(1) && home.contains(2) &&
            uniform(home, 2),
        "C1: a student whose area has open schools must pick uniformly "
        "among exactly those");

  const auto closing = tally(kClosingArea);
  print("C2 home area 7 (its school closes mid-plan)", closing);
  check(closing.size() == 3 && closing.contains(1) && closing.contains(2) &&
            closing.contains(6) && uniform(closing, 3),
        "C2: a school closing mid-plan is no option; the fallback must be "
        "uniform over every school open throughout, online included");

  const auto opening = tally(kOpeningArea);
  print("C3 home area 9 (its school opens mid-plan)", opening);
  check(opening == closing,
        "C3: a school opening mid-plan is no option either");

  const auto nowhere = tally(geo::invalidGeoArea);
  print("C4 no home area", nowhere);
  check(nowhere == closing, "C4: no home area must take the national pool");

  // C5: one draw when a school is picked, none when nothing is open.
  auto drawn = pl::random::Rng::fromSeed(99);
  auto reference = pl::random::Rng::fromSeed(99);
  (void)directory.pick(kHome, kFirst, kLast, drawn);
  (void)reference.uniformInt(0, 2);
  check(sameState(drawn, reference),
        "C5: a pick must spend exactly one bounded draw over the pool");

  pl::entity::merchant::Catalog shut;
  shut.records = {educationRecord(1, kHome, kNever, kFirst),
                  educationRecord(2, geo::invalidGeoArea, kLast + 1, kAlways)};
  const schools::Directory none{shut};
  auto idle = pl::random::Rng::fromSeed(99);
  const auto picked = none.pick(kHome, kFirst, kLast, idle);
  std::printf("  C5 nothing open: %s, lane %s\n",
              picked.has_value() ? "PICKED" : "no school",
              sameState(idle, pl::random::Rng::fromSeed(99)) ? "untouched"
                                                             : "DRAWN");
  check(!picked.has_value(), "C5: a pick with nothing open must return none");
  check(sameState(idle, pl::random::Rng::fromSeed(99)),
        "C5: a pick with nothing open must spend no draw");
}

// ------------------------------------------------------------------ D

void gateD(const pl::synth::pii::PoolSet &poolSet) {
  std::printf("\n##### D: the settled corpus at the run-golden config #####\n");
  pltest::LegOptions options{};
  options.population = 2'000;
  options.window =
      pl::time::Window{.start = pl::time::makeTime({2025, 1, 1}), .days = 60};
  options.seed = kGoldenSeed;
  options.withFamily = true;
  const auto leg = pltest::runLeg(poolSet, options);

  const auto &catalog = leg.merchants;
  std::unordered_map<Key, std::size_t, std::hash<Key>> recordOf;
  for (std::size_t i = 0; i < catalog.records.size(); ++i) {
    if (catalog.records[i].category == Category::education) {
      recordOf.emplace(catalog.records[i].counterpartyId, i);
    }
  }

  std::size_t tuitionRows = 0;
  std::size_t offDomain = 0;
  std::size_t closedAtRow = 0;
  std::unordered_map<Key, std::size_t, std::hash<Key>> rowsBySchool;
  for (const auto &row : leg.rows) {
    if (!isTuition(row)) {
      continue;
    }
    ++tuitionRows;
    ++rowsBySchool[row.target];
    const auto it = recordOf.find(row.target);
    if (it == recordOf.end()) {
      ++offDomain;
    } else if (!catalog.records[it->second].liveAt(row.timestamp)) {
      ++closedAtRow;
    }
  }
  std::size_t largest = 0;
  for (const auto &[key, n] : rowsBySchool) {
    largest = std::max(largest, n);
  }
  const double share = tuitionRows == 0 ? 1.0
                                        : static_cast<double>(largest) /
                                              static_cast<double>(tuitionRows);
  std::printf("  %zu corpus rows, %zu tuition rows settled over %zu schools; "
              "off the education catalogue %zu, closed at the row %zu; "
              "largest school %zu rows, share %.4f (bound %.2f)\n",
              leg.rows.size(), tuitionRows, rowsBySchool.size(), offDomain,
              closedAtRow, largest, share, kMaxSchoolShare);
  check(tuitionRows > 0, "D: the settled corpus carries no tuition row");
  check(offDomain == 0,
        "D: a settled tuition row pays an account that is not an education "
        "catalogue record");
  check(closedAtRow == 0,
        "D: a settled tuition row pays a school closed at its date");
  check(rowsBySchool.size() >= 2 && share < 1.0,
        "D: one account takes every settled tuition row");
  check(share <= kMaxSchoolShare,
        "D: the largest school's share of settled tuition rows is above the "
        "bound");
}

} // namespace

int main() {
  const auto poolSet = pltest::buildPoolSet(kGoldenSeed);
  gateA(poolSet);

  pltest::WorldSpec golden;
  golden.seed = kGoldenSeed;
  golden.window =
      pl::time::Window{.start = pl::time::makeTime({2025, 1, 1}), .days = 60};
  golden.population = 2'000;
  golden.fraudProfile = pltest::scaledFraudProfile();
  golden.withIncome = false;
  gateB(poolSet, "run-golden world", golden, kMaxSchoolShare, 25, 0.0);

  pltest::WorldSpec scale;
  scale.seed = 42;
  scale.window =
      pl::time::Window{.start = pl::time::makeTime({2024, 1, 1}), .days = 366};
  scale.population = 200'000;
  scale.fraudProfile = pltest::scaledFraudProfile();
  scale.withIncome = false;
  scale.withProducts = false;
  gateB(poolSet, "scale leg", scale, kMaxSchoolShare, 4'000, 100.0);

  gateC();
  gateD(poolSet);

  if (g_failures != 0) {
    std::fprintf(stderr, "\n%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("\nall tuition payee checks passed\n");
  return 0;
}
