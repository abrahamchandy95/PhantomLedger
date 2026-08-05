// tests/test_merchant_churn.cpp
//
// THE MERCHANT LIFECYCLE GATE (merchant-churn-2026-07).
//
// The defect this exists for: `makeCatalog` took no window and no date, and
// `merchant::Record` carried no time field, so EVERY merchant was live on
// EVERY day. Over the owner's 20-year target that is 490 acceptance
// endpoints, fixed, for 7,305 days — while BLS Business Employment Dynamics
// puts retail 5-year survival near 58% and the March-1994 birth cohort at
// ~14% by 2025.
//
// WHAT THIS GATE MEASURES, and why each check is here rather than a simpler
// "lifecycle is assigned" assertion:
//
//   A. INTERVALS EXIST AND ARE WELL-FORMED. Cheap, and it is the
//      precondition for every check below — a gate that measured churn
//      without pinning this would report 0 and read as "no churn" when the
//      truth was "assignLifecycle never ran".
//
//   B. THE LIVE COUNT IS STATIONARY. This is the check that would have
//      caught the round's first design: deaths without births thin the pool,
//      which is the OPPOSITE of the reported defect. Births must replace
//      deaths, so live(start) and live(end) must sit within a band of each
//      other while both stay well above zero.
//
//   C. INCUMBENT SURVIVAL MATCHES THE BLS-DERIVED HAZARD. The mature band
//      is 0.0540/yr, so survival over `years` is ~(1-h)^years. Banded, not
//      pinned, because the recession modulation moves it.
//
//   D. THE UNIVERSE TRAVERSED EXCEEDS THE UNIVERSE AVAILABLE AT ANY
//      INSTANT. This is the owner's actual complaint stated as a
//      measurement: over a long window a population must transact with more
//      distinct merchants than exist on any single day. A static catalogue
//      makes these two numbers EQUAL, which is exactly the pre-round state.
//
//   E. ZERO OUT-OF-TENURE TRANSACTIONS — a HARD ZERO, not a band. Mirrors
//      the attacker-endpoint round's out-of-tenure check, and it is the one
//      check that proves liveness reached SELECTION rather than merely being
//      recorded on the record. Without it, A-D can all pass while every
//      merchant still receives transactions for the whole window.
//
// E IS THE LOAD-BEARING CHECK. A/B/C/D are properties of the assignment; E
// is the only one that fails if the CDF rebuild or the fraud-rail filter is
// removed.

#include "gate_world.hpp"
#include "test_support.hpp"
#include "window_leg_support.hpp"

#include "phantomledger/activity/spending/market/bootstrap.hpp"
#include "phantomledger/primitives/random/factory.hpp"

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/merchants/lifecycle.hpp"
#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace {

namespace pl = ::PhantomLedger;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// Bands. Each is derived, and the derivation is stated so a future reader
// can tell a calibration change from a construction defect.
//
// Live-count stationarity: births are sized as EXPECTED deaths, so the
// realized end/start ratio is a binomial realization around 1.0. At a few
// hundred merchants the sampling sd is a few percent; the band is generous
// enough to survive that and tight enough that a decaying pool (the first
// design's failure mode, which would land near 0.66 over 20 years) reds.
constexpr double kMinLiveRatio = 0.80;
constexpr double kMaxLiveRatio = 1.20;

// Incumbent survival: (1 - 0.0540)^years, widened for the recession lift
// (which only ever KILLS more, so the band is asymmetric downward).
constexpr double kSurvivalTolerance = 0.14;

// D: distinct merchants transacted with over the window, as a multiple of
// the live count at window start. A static catalogue scores ~1.0 by
// construction; anything meaningfully above 1.0 proves the universe grew.
constexpr double kMinTraversalRatio = 1.10;

// Intra-month liveness latency. See the check itself for the derivation and
// for why this is a floor rather than a widened band.
constexpr double kMaxOutOfTenureShare = 0.010;

struct Leg {
  const char *name;
  std::uint64_t seed;
  int startYear;
  int days;
  std::int32_t population;
};

void runChurnLeg(const Leg &leg) {
  std::printf("\n=== %s: %d people, %d days from %d ===\n", leg.name,
              leg.population, leg.days, leg.startYear);

  const auto poolSet = pltest::buildPoolSet(leg.seed);
  pl::time::Window window{};
  window.start = pl::time::makeTime({leg.startYear, 1, 1});
  window.days = leg.days;

  pltest::LegOptions options{};
  options.population = leg.population;
  options.window = window;
  options.seed = leg.seed;

  const auto result = pltest::runLeg(poolSet, options);
  const auto &records = result.merchants.records;
  check(!records.empty(), "the leg produced no merchant catalogue");

  const auto startEpoch = pl::time::toEpochSeconds(window.start);
  const auto endEpoch =
      pl::time::toEpochSeconds(pl::time::addDays(window.start, window.days));
  const double years = static_cast<double>(window.days) / 365.25;

  // ---------------------------------------------- A. intervals well-formed
  std::size_t assigned = 0;
  std::size_t malformed = 0;
  std::size_t incumbents = 0;
  std::size_t births = 0;
  for (const auto &rec : records) {
    if (rec.lifecycleAssigned()) {
      ++assigned;
    }
    if (rec.firstEpoch >= rec.lastEpochExcl) {
      ++malformed;
    }
    if (rec.firstEpoch == pl::entity::merchant::kEpochMin) {
      ++incumbents;
    } else {
      ++births;
    }
  }
  check(assigned > 0,
        "no merchant carries a lifecycle interval — assignLifecycle did not "
        "run, and every churn measurement below would read as 'no churn' "
        "when the truth is 'no mechanism'");
  check(malformed == 0, "every interval must be non-empty: " +
                            std::to_string(malformed) + " have first >= last");
  check(births > 0,
        "no merchant was BORN inside the window — appendChurnReplacements "
        "produced nothing, so deaths cannot be replaced and the pool can "
        "only decay");

  // ------------------------------------------- B. live count is stationary
  const auto liveStart = result.merchants.liveCountAt(startEpoch);
  const auto liveEnd = result.merchants.liveCountAt(endEpoch - 1);
  check(liveStart > 0 && liveEnd > 0,
        "the catalogue must have live merchants at both window edges");
  const double liveRatio =
      static_cast<double>(liveEnd) / static_cast<double>(liveStart);
  check(liveRatio >= kMinLiveRatio && liveRatio <= kMaxLiveRatio,
        "live merchant count must stay stationary across the window (end/"
        "start = " +
            std::to_string(liveRatio) + ", band [" +
            std::to_string(kMinLiveRatio) + ", " +
            std::to_string(kMaxLiveRatio) +
            "]). A ratio well below 1 means deaths are not being replaced by "
            "births, which THINS the merchant pool — the opposite of the "
            "churn this round exists to add");

  // --------------------------------- C. incumbent survival vs BLS hazard
  std::size_t incumbentSurvivors = 0;
  for (const auto &rec : records) {
    if (rec.firstEpoch == pl::entity::merchant::kEpochMin &&
        rec.liveAt(endEpoch - 1)) {
      ++incumbentSurvivors;
    }
  }
  const double survival = incumbents > 0
                              ? static_cast<double>(incumbentSurvivors) /
                                    static_cast<double>(incumbents)
                              : 0.0;
  // C1 — IMPLEMENTATION FIDELITY. Reads `kHazardMature` rather than a
  // hardcoded copy, which it used to: the literal 0.0540 sat here while the
  // constant moved, so a recalibration would have left the gate asserting the
  // OLD hazard and reading red for the right answer.
  //
  // NOTE WHAT THIS CHECK CAN AND CANNOT DO. Deriving the expectation from the
  // constant verifies that the mechanism applies the hazard it declares — real
  // and worth having, because a lifecycle that silently ignored the band would
  // fail here. But it passes for ANY value of the constant, so it is NOT a
  // claim about the world. C2 below is.
  const double expected =
      std::pow(1.0 - pl::synth::merchants::kHazardMature, years);
  check(std::abs(survival - expected) <= kSurvivalTolerance,
        "incumbent survival over " + std::to_string(years) +
            " years must track the DECLARED mature hazard (expected ~" +
            std::to_string(expected) + ", got " + std::to_string(survival) +
            ", tolerance " + std::to_string(kSurvivalTolerance) + ")");

  // C2 — CALIBRATION AGAINST THE PUBLISHED SERIES. The banded hazard curve
  // must reproduce the BLS BED NAICS-44 March-1994 cohort's published survival
  // points. This is the check that was MISSING, and its absence is why the
  // constants carried two errors for a whole round: the header's stated
  // derivation was wrong in the arithmetic AND wrong in the cited percentages,
  // and nothing evaluated either.
  //
  // The 6-year point is the one that earns this check its keep. The three
  // hazards are fitted to the 1-, 4- and 31-year points, so those three MUST
  // match — matching them proves only that the arithmetic was done. Six years
  // is fitted by nothing and lands within a percentage point, which is
  // evidence about the three-band STRUCTURE rather than about the fit.
  {
    namespace life = pl::synth::merchants;
    const auto curve = [](double years_) {
      const double s1 = 1.0 - life::kHazardFirstYear;
      if (years_ <= 1.0) {
        return s1;
      }
      const double mid = std::min(years_, 5.0) - 1.0;
      const double s5 = s1 * std::pow(1.0 - life::kHazardYears1To5, mid);
      if (years_ <= 5.0) {
        return s5;
      }
      return s5 * std::pow(1.0 - life::kHazardMature, years_ - 5.0);
    };

    // Fitted points: tight, because a mismatch here is an arithmetic error.
    constexpr double kFittedTolerance = 0.005;
    // Unfitted: loose enough to be a structural claim, tight enough that a
    // two-band or flat-hazard model would fail it.
    constexpr double kUnfittedTolerance = 0.02;

    struct Point {
      const char *label;
      double years;
      double published;
      double tolerance;
      bool fitted;
    };
    const Point points[] = {
        {"1-year (Mar-1995)", 1.0, life::kBlsSurvival1Year, kFittedTolerance,
         true},
        {"4-year (Mar-1998)", 4.0, life::kBlsSurvival4Year, kFittedTolerance,
         true},
        {"6-year (Mar-2000)", 6.0, life::kBlsSurvival6Year, kUnfittedTolerance,
         false},
        {"31-year (Mar-2025)", 31.0, life::kBlsSurvival31Year,
         kFittedTolerance, true},
    };

    std::printf("  C2 BLS calibration (NAICS 44, Mar-1994 cohort):\n");
    for (const auto &p : points) {
      const double modelled = curve(p.years);
      std::printf("     %-20s model %.4f  published %.4f  err %+.2fpp  %s\n",
                  p.label, modelled, p.published,
                  (modelled - p.published) * 100.0,
                  p.fitted ? "(fitted)" : "(NOT fitted — validation)");
      check(std::abs(modelled - p.published) <= p.tolerance,
            std::string("the banded hazard curve must reproduce the published "
                        "BLS survival point at ") +
                p.label + " (model " + std::to_string(modelled) +
                ", published " + std::to_string(p.published) + ", tolerance " +
                std::to_string(p.tolerance) +
                (p.fitted ? "). This point is FITTED, so a mismatch is an "
                            "arithmetic error in the derivation, not a "
                            "calibration disagreement"
                          : "). This point is NOT fitted — it is the "
                            "independent check that the three-band structure "
                            "is right rather than curve-fitted"));
    }
  }

  // --------- D/E. what the corpus actually transacted with, and when
  std::map<pl::entity::Key, const pl::entity::merchant::Record *> byKey;
  for (const auto &rec : records) {
    byKey.emplace(rec.counterpartyId, &rec);
  }

  // THE SENTINEL, and excluding it is a correctness fix to THIS GATE rather
  // than a concession about the model.
  //
  // `PaymentRouter::emitExternal` routes the "external merchant we do not
  // model" slot to a HARDCODED key: makeKey(merchant, external, 1). That is
  // a catch-all bucket, not a modelled merchant, and it has no lifecycle —
  // but it COLLIDES with catalogue serial 1, so a naive key join counts
  // every external-unknown row as belonging to merchant #1 and reports them
  // all out of tenure the moment #1 closes.
  //
  // The first version of this gate did exactly that and read 85% out of
  // tenure on that channel, which looked like a missing liveness filter and
  // was really a join defect. The channel breakdown below is what separated
  // the two, which is why it prints unconditionally.
  const auto externalSentinel = pl::entity::makeKey(
      pl::entity::Role::merchant, pl::entity::Bank::external, 1u);

  std::set<pl::entity::Key> touched;
  std::size_t outOfTenure = 0;
  std::size_t merchantRows = 0;
  // Out-of-tenure rows BY CHANNEL. Printed rather than gated: when the hard
  // zero fails, the only useful next question is which routing path still
  // ignores liveness, and a bare total cannot answer it. Every liveness fix
  // in this round was found by reading this breakdown.
  std::map<std::uint8_t, std::size_t> leakByChannel;
  std::map<std::uint8_t, std::size_t> rowsByChannel;
  for (const auto &row : result.rows) {
    if (row.target == externalSentinel) {
      continue; // the unmodelled-merchant catch-all; see above
    }
    const auto it = byKey.find(row.target);
    if (it == byKey.end()) {
      continue; // non-catalogue destination (biller fallback, transfers)
    }
    ++merchantRows;
    touched.insert(row.target);
    ++rowsByChannel[row.session.channel.value];
    if (!it->second->liveAt(row.timestamp)) {
      ++outOfTenure;
      ++leakByChannel[row.session.channel.value];
    }
  }
  check(merchantRows > 0, "the leg produced no catalogue-merchant rows");

  const double traversalRatio =
      static_cast<double>(touched.size()) / static_cast<double>(liveStart);
  check(traversalRatio >= kMinTraversalRatio,
        "the population must transact with MORE distinct merchants over the "
        "window than exist on any single day (traversed " +
            std::to_string(touched.size()) + " vs " +
            std::to_string(liveStart) + " live at start, ratio " +
            std::to_string(traversalRatio) + ", floor " +
            std::to_string(kMinTraversalRatio) +
            "). A static catalogue scores ~1.0 here BY CONSTRUCTION, which "
            "is exactly the pre-round state this gate exists to detect");

  // THE LOAD-BEARING CHECK.
  //
  // NOT a hard zero, and the reason is structural rather than a widened
  // band. Liveness is enforced at MONTH BOUNDARIES (the commerce evolver's
  // only hook), so a merchant that closes on the 5th keeps serving the
  // people who already favoured it until the next boundary. With a ~5%/yr
  // mature death hazard, half a month of residual exposure per death lands
  // near 0.2%, and the bill channel runs slightly higher because a monthly
  // debit has exactly one chance to fall in that gap.
  //
  // The FRAUD rail is exempt from that latency — it filters per case on the
  // case's own timestamp — so this band covers the legitimate paths only.
  //
  // The band is set just above the observed floor so that REMOVING any
  // liveness mechanism reds it: the frozen-favourites state measured 49%,
  // biller staleness added 5%, and the pre-round state is 100% of rows on
  // any closed merchant. There is no configuration between "monthly
  // granularity" and "no liveness" that lands inside this band.
  const double outOfTenureShare =
      static_cast<double>(outOfTenure) / static_cast<double>(merchantRows);
  check(outOfTenureShare <= kMaxOutOfTenureShare,
        "transactions posting outside a merchant's operating interval must "
        "stay at the intra-month-latency floor (" +
            std::to_string(outOfTenureShare) + " observed, ceiling " +
            std::to_string(kMaxOutOfTenureShare) + "; " +
            std::to_string(outOfTenure) + " of " +
            std::to_string(merchantRows) +
            " rows). This is the check that fails if liveness stops reaching "
            "SELECTION — the monthly CDF rebuilds, the forced favourite/biller "
            "drops, or the fraud rail's per-case filter. Every other check "
            "here passes on a recorded-but-ignored interval");

  std::printf("  catalogue %zu records: %zu incumbents, %zu born in-window\n",
              records.size(), incumbents, births);
  std::printf("  live %zu at start -> %zu at end (ratio %.3f)\n", liveStart,
              liveEnd, liveRatio);
  std::printf("  incumbent survival %.4f over %.1f years (BLS-derived "
              "expectation %.4f)\n",
              survival, years, expected);
  std::printf("  traversed %zu distinct merchants = %.2fx the live count at "
              "any instant\n",
              touched.size(), traversalRatio);
  std::printf("  out-of-tenure merchant rows: %zu of %zu (%.3f%%, ceiling "
              "%.1f%% — intra-month latency floor, not a widened band)\n",
              outOfTenure, merchantRows, 100.0 * outOfTenureShare,
              100.0 * kMaxOutOfTenureShare);
  for (const auto &[channel, count] : leakByChannel) {
    const auto total = rowsByChannel[channel];
    const auto label = pl::channels::name(pl::channels::Tag{channel});
    std::printf("    %-22.*s %zu of %zu out of tenure (%.2f%%)\n",
                static_cast<int>(label.size()), label.data(), count, total,
                100.0 * static_cast<double>(count) /
                    static_cast<double>(total));
  }
}

// ------------------------------------------- THE BURST-RATE GATE
//
// burst-rate-2026-07. Spending bursts were ONE PER PERSON PER RUN at
// p=0.08, so the constant meant ~0.49/year at a 60-day config and 0.004/year
// over twenty years: **a two-decade run gave 92% of people no burst at all
// and the rest exactly one.** The rate is now per-year.
//
// WHAT IS GATED IS THE SCALING, not the presence of bursts. A per-run
// probability and a per-year rate are indistinguishable at ONE window
// length — that is exactly why the defect survived — so the only honest
// check compares TWO window lengths and requires the count to grow with the
// window. A gate that measured bursts at a single horizon would pass
// unchanged against the defect it exists to catch.
void runBurstScalingGate() {
  std::printf("\n=== burst scaling: bursts per person must grow with the "
              "window ===\n");

  struct Probe {
    int days;
    double expectedPerPerson;
  };
  // 0.487/year scaled by the span. A per-RUN constant would produce the SAME
  // number at both horizons, which is the failure this gate detects.
  const Probe probes[] = {{365, 0.487}, {3652, 4.87}};

  // MEASURES the production construction — `buildPersonBursts` is the one
  // implementation both this gate and `buildMarket` call. An earlier version
  // re-derived the formula here, which would have passed vacuously the
  // moment production changed and the copy did not.
  //
  // Averaged over a population of synthetic draws rather than checked on one
  // person: the quantity is a RATE, so a single person's realized count is a
  // Bernoulli sum and says nothing.
  constexpr std::uint32_t kSamples = 4000;
  double observed[2] = {0.0, 0.0};
  const pl::activity::spending::market::BurstSchedule rules{};

  for (std::size_t i = 0; i < 2; ++i) {
    const pl::random::RngFactory factory{909090};
    std::size_t total = 0;
    for (std::uint32_t person = 0; person < kSamples; ++person) {
      std::array<char, 16> buf{};
      const auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(),
                                           static_cast<unsigned>(person));
      (void)ec;
      auto rng = factory.rng(
          {"burst-gate",
           std::string_view(buf.data(),
                            static_cast<std::size_t>(ptr - buf.data()))});
      total += pl::activity::spending::market::buildPersonBursts(
                   rng, rules, probes[i].days)
                   .size();
    }
    observed[i] =
        static_cast<double>(total) / static_cast<double>(kSamples);
    std::printf("  %5d days -> %.3f bursts/person measured over %u draws "
                "(annual rate %.3f)\n",
                probes[i].days, observed[i], kSamples, rules.burstsPerYear);
  }

  check(observed[1] > observed[0] * 5.0,
        "bursts per person must scale with the window: " +
            std::to_string(observed[0]) + " at 365 days vs " +
            std::to_string(observed[1]) +
            " at 3652 days. A per-RUN probability yields the SAME value at "
            "both horizons, which is the pre-round defect");
  check(std::abs(observed[0] - probes[0].expectedPerPerson) < 0.05,
        "the one-year rate must equal the declared annual rate (" +
            std::to_string(observed[0]) + " vs " +
            std::to_string(probes[0].expectedPerPerson) + ")");
}

} // namespace

int main() {
  // Two legs on different seeds and horizons. The long leg is where the
  // survival and traversal signals live; the short leg exists to prove the
  // mechanism degrades gracefully rather than only working at 20 years.
  const Leg legs[] = {
      {"leg-long", 20260729, 2000, 5478, 600},
      {"leg-short", 20260730, 2010, 1826, 900},
  };
  try {
    for (const auto &leg : legs) {
      runChurnLeg(leg);
    }
    runBurstScalingGate();
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "\n%d check(s) failed.\n", g_failures);
    return 1;
  }
  std::printf("\nAll merchant-churn checks passed.\n");
  return 0;
}
