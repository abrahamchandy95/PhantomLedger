// tests/test_card_churn.cpp
//
// THE CARD REISSUE GATE (card-churn-2026-07).
//
// The defect: the exported card number is `'C'/'D' + format(accountKey)`, a
// pure function of the account, so **one account carried exactly one card
// number for the whole run** — no cardholder ever received a replacement in
// twenty years. Auriemma Consulting Group reports ~50% of US cardholders
// getting at least one reissuance within a YEAR.
//
// This gate measures `entity::card::reissue` DIRECTLY rather than through a
// corpus run. That is deliberate and it is the same reasoning the burst gate
// uses: the quantity under test is a per-card COUNT and a schedule shape,
// and the corpus only shows its downstream effect on `cf_Card` cardinality,
// which the observed-card set also moves. Measuring the rule keeps the gate
// pointed at the mechanism.
//
// WHAT IS CHECKED, and why each one earns its place:
//
//   A. THE SEQUENCE TILES THE WINDOW — contiguous, no gap, no overlap. A gap
//      would leave transactions inside it with no resolvable card number,
//      which the exporter cannot represent; an overlap would make
//      `generationAt` ambiguous. This is a HARD structural check.
//
//   B. CHURN ACTUALLY HAPPENS over a long window. The pre-round state is
//      exactly one generation per card, so this is the check that fails
//      against the defect.
//
//   C. OVER-DISPERSION OF THE PRONENESS MIXTURE, not of the total count.
//      The first version of this check compared variance against mean on the
//      TOTAL generations per card and failed at 2.1 vs 6.7 — correctly, but
//      the check was wrong rather than the model: the total is dominated by
//      the near-DETERMINISTIC expiry term (~5 generations, validity only
//      varying 36-60 months), which contributes almost no variance. Total
//      variance < mean is the expected signature of a mostly-scheduled
//      process.
//
//      The over-dispersion claim is about the UNSCHEDULED component, so that
//      is what is measured: the mixture must have mean ~1 (it rescales the
//      base rate without moving the population average, which is what lets
//      the base rate stay the value derived from Auriemma's shares) and a
//      heavy UPPER TAIL, since "~50% get one a year" and "repeat victims get
//      ~5" cannot come from one flat rate.
//
//   D. SHORT WINDOWS STAY QUIET. Over 60 days almost no card should turn
//      over, so a config calibrated at that horizon is not disturbed. This
//      is the counterpart to B: together they pin the SCALING, which a
//      single horizon cannot.
//
//   E. THE EMV WAVE lands inside the migration years and nowhere else.

#include "test_support.hpp"

#include "phantomledger/entities/holdings/card_reissue.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace reissue = pl::entity::card::reissue;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// Bands. Each derivation is stated so a later reader can separate a
// calibration change from a construction defect.
//
// Over 20 years: expiry alone gives 20/(validity 3-5y) = 4-6.7 generations,
// plus the unscheduled hazard and at most one EMV conversion. The floor is
// set below the pure-expiry minimum so a slow-expiry seed cannot red it, and
// the ceiling well above so the heavy tail is not clipped into a failure.
constexpr double kMinMeanGenerations20y = 4.0;
constexpr double kMaxMeanGenerations20y = 9.0;

// Over 60 days, expiry cannot fire (minimum term is 3 years) and the
// unscheduled rate is ~0.07/yr, so essentially nothing should turn over.
constexpr double kMaxChurnShare60d = 0.05;

[[nodiscard]] pl::entity::Key cardKey(std::uint64_t serial) {
  return pl::entity::makeKey(pl::entity::Role::card,
                             pl::entity::Bank::internal, serial);
}

struct Stats {
  double mean = 0.0;
  double variance = 0.0;
  double multiShare = 0.0;
  std::size_t structuralFailures = 0;
};

[[nodiscard]] Stats measure(std::int64_t start, std::int64_t endExcl,
                            std::uint64_t cards) {
  Stats out;
  std::vector<double> counts;
  counts.reserve(cards);

  for (std::uint64_t i = 0; i < cards; ++i) {
    const auto key = cardKey(1'000 + i);
    const auto generations = reissue::generationsFor(key, start, endExcl);

    // A. structural: tiles the window exactly, in order, no gap or overlap.
    if (generations.empty()) {
      ++out.structuralFailures;
      counts.push_back(0.0);
      continue;
    }
    if (generations.front().firstEpoch != start ||
        generations.back().lastEpochExcl != endExcl) {
      ++out.structuralFailures;
    }
    for (std::size_t g = 1; g < generations.size(); ++g) {
      if (generations[g].firstEpoch != generations[g - 1].lastEpochExcl) {
        ++out.structuralFailures;
        break;
      }
    }
    for (const auto &generation : generations) {
      if (generation.firstEpoch >= generation.lastEpochExcl) {
        ++out.structuralFailures;
        break;
      }
    }

    counts.push_back(static_cast<double>(generations.size()));
    if (generations.size() > 1) {
      out.multiShare += 1.0;
    }
  }

  const auto n = static_cast<double>(counts.size());
  for (const auto c : counts) {
    out.mean += c;
  }
  out.mean /= n;
  for (const auto c : counts) {
    out.variance += (c - out.mean) * (c - out.mean);
  }
  out.variance /= n;
  out.multiShare /= n;
  return out;
}

} // namespace

int main() {
  try {
    constexpr std::uint64_t kCards = 4000;

    const auto y2000 = pl::time::toEpochSeconds(pl::time::makeTime({2000, 1, 1}));
    const auto y2020 = pl::time::toEpochSeconds(pl::time::makeTime({2020, 1, 1}));
    const auto d60 =
        pl::time::toEpochSeconds(pl::time::makeTime({2000, 3, 1}));

    std::printf("=== 20-year window (2000-2020), %llu cards ===\n",
                static_cast<unsigned long long>(kCards));
    const auto longRun = measure(y2000, y2020, kCards);
    std::printf("  generations/card: mean %.3f, variance %.3f, "
                ">1 generation %.3f\n",
                longRun.mean, longRun.variance, longRun.multiShare);

    // A
    check(longRun.structuralFailures == 0,
          "every generation sequence must TILE the window contiguously (" +
              std::to_string(longRun.structuralFailures) +
              " cards violate it). A gap leaves transactions with no "
              "resolvable card number; an overlap makes generationAt "
              "ambiguous");
    // B
    check(longRun.mean >= kMinMeanGenerations20y &&
              longRun.mean <= kMaxMeanGenerations20y,
          "mean generations per card over 20 years must sit in [" +
              std::to_string(kMinMeanGenerations20y) + ", " +
              std::to_string(kMaxMeanGenerations20y) + "], got " +
              std::to_string(longRun.mean) +
              ". The pre-round state is EXACTLY 1.0 — one card number per "
              "account for the whole run");
    // C — the mixture itself, which is what the over-dispersion claim is
    // about. See the header note on why the TOTAL count is the wrong
    // statistic here.
    std::vector<double> prone;
    prone.reserve(kCards);
    for (std::uint64_t i = 0; i < kCards; ++i) {
      prone.push_back(reissue::proneness(cardKey(1'000 + i)));
    }
    std::ranges::sort(prone);
    double proneMean = 0.0;
    for (const auto v : prone) {
      proneMean += v;
    }
    proneMean /= static_cast<double>(prone.size());
    const auto median = prone[prone.size() / 2];
    const auto p99 = prone[(prone.size() * 99) / 100];
    std::printf("  proneness: mean %.3f, median %.3f, p99 %.3f (p99/median "
                "%.2fx)\n",
                proneMean, median, p99, p99 / median);

    check(std::abs(proneMean - 1.0) < 0.10,
          "the proneness mixture must have mean ~1 so it rescales the base "
          "rate without moving the population average (got " +
              std::to_string(proneMean) +
              "). A mean above 1 would silently inflate the churn rate away "
              "from the value derived from Auriemma's cause shares");
    check(p99 / median >= 3.5,
          "the proneness mixture must carry a heavy UPPER TAIL: p99/median " +
              std::to_string(p99 / median) +
              " must be at least 3.5x. A flat rate scores 1.0x and cannot "
              "produce both '~50% of cardholders get one a year' and "
              "'repeat-fraud victims get ~5'");

    std::printf("\n=== 60-day window, %llu cards ===\n",
                static_cast<unsigned long long>(kCards));
    const auto shortRun = measure(y2000, d60, kCards);
    std::printf("  generations/card: mean %.3f, >1 generation %.3f\n",
                shortRun.mean, shortRun.multiShare);
    // D
    check(shortRun.structuralFailures == 0,
          "the 60-day sequences must tile too (" +
              std::to_string(shortRun.structuralFailures) + " violate it)");
    check(shortRun.multiShare <= kMaxChurnShare60d,
          "almost no card should turn over in 60 days (share with >1 "
          "generation " +
              std::to_string(shortRun.multiShare) + ", ceiling " +
              std::to_string(kMaxChurnShare60d) +
              "). Expiry cannot fire below a 3-year term, so a high value "
              "here means the rate is being applied without its duration — "
              "the same arithmetic slip the burst rate made");

    // E
    std::size_t inWave = 0;
    std::size_t emvCards = 0;
    for (std::uint64_t i = 0; i < kCards; ++i) {
      const auto key = cardKey(1'000 + i);
      if (!reissue::inEmvWave(key)) {
        continue;
      }
      ++emvCards;
      const auto generations = reissue::generationsFor(key, y2000, y2020);
      for (std::size_t g = 1; g < generations.size(); ++g) {
        const auto boundary = generations[g].firstEpoch;
        const auto year = pl::time::toCalendarDate(
                              pl::time::fromEpochSeconds(boundary))
                              .year;
        if (year >= reissue::kEmvWaveFirstYear &&
            year <= reissue::kEmvWaveLastYear) {
          ++inWave;
          break;
        }
      }
    }
    std::printf("\n=== EMV wave ===\n");
    std::printf("  %zu of %llu cards in the wave; %zu have a boundary inside "
                "%d-%d\n",
                emvCards, static_cast<unsigned long long>(kCards), inWave,
                reissue::kEmvWaveFirstYear, reissue::kEmvWaveLastYear);
    check(emvCards > 0, "the EMV wave must select some cards");
    check(inWave > 0,
          "cards in the EMV wave must carry a generation boundary inside the "
          "migration years");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "\n%d check(s) failed.\n", g_failures);
    return 1;
  }
  std::printf("\nAll card-churn checks passed.\n");
  return 0;
}
