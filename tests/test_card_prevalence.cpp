//
// tests/test_card_prevalence.cpp
//
// card-fraud-realism-v2 ROUND 3: THE PREVALENCE SUITE — gate 3 of
// docs/card_fraud_online_gnn.md. Before this gate the corpus had ONE
// fraud number (an aggregate rate over a whole window) and that number
// stopped meaning anything the moment macro-history H4 made activity
// volume era-varying. This suite replaces it with a per-year,
// per-channel, per-typology, per-amount, per-episode picture on THE
// CARD VIEW — the exact rows that become cf_Payment_Transaction.
//
// THE DESIGN PRINCIPLE, learned the hard way in H4: a 300-person gate
// world carries a documented ~27% second-year liquidity drain (U-9
// ADDENDUM — income under-provision at small N). So this suite GATES
// only what the harness can resolve and PRINTS everything else. In
// particular it does NOT gate the direction of per-year row counts;
// the drain and the H4 real-consumption ramp push on the same number.
//
// WHAT IS GATED, and why each one can fail for a real reason:
//
//   RATE STABILITY ACROSS YEARS. The fraud budget is F = pL/(1-p),
//   riding the REALIZED candidate count L. So when H4 makes a 1991
//   window quieter, fraud counts fall with it and the RATE holds. If
//   someone ever keys a fraud budget to a window constant or to
//   population instead of L, the yearly rate fans out and this gate
//   catches it.
//
//   CHANNEL STRUCTURE. Unauthorized card fraud and the gift-card scam
//   ride card_purchase. The `merchant` channel (account-paid POS, read
//   as debit) carries essentially none. A drift here means the view
//   filter or the fraud rail's channel choice moved.
//
//   TYPOLOGY PLURALITY. The card rail must not become a monoculture:
//   at least two distinct fraud types, with the unauthorized family
//   dominant (it is .60 of the unauthorized mix).
//
//   AMOUNT DIRECTION + DEFLATED STABILITY. Fraud tickets are larger
//   than legitimate ones, and their era scaling is CPI-realized (U-6
//   class F), so per-year fraud means DEFLATED by priceScale(year)
//   should be roughly flat. This is the only gate that checks the H1
//   amount wiring actually reaches the card fraud rail.
//
//   EPISODE SIZE. Rows per compromised card, bounded. An unauthorized
//   episode is a handful of transactions, not a career.
//
// The aggregate rate carries a NAMED COMPARATOR rather than a tight
// band: IBM TabFormer observed 28,471 fraud rows in 24,386,900
// (0.11675%). PhantomLedger is TabFormer-SHAPED, not calibrated to it,
// so the gate is a wide plausibility band and the ratio is PRINTED for
// the record.
//

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"

#include "window_leg_support.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;
namespace econ = pl::synth::econ;

using pltest::LegOptions;
using Txn = pltest::Txn;

namespace {

constexpr std::uint64_t kSeed = 1234567;
constexpr std::int32_t kPopulation = 300;
// 1991-01-01 + 1461 days = 1995-01-01: four WHOLE calendar years
// (1991, 1992 with its leap day, 1993, 1994), all in-era.
constexpr int kDays = 1461;

// IBM TabFormer's observed transaction-fraud prevalence. A NAMED
// COMPARATOR, not a calibration target.
constexpr double kTabFormerRate = 0.0011675;

// Wide plausibility band around card-fraud prevalence: 0.02% to 2%.
constexpr double kRateFloor = 0.0002;
constexpr double kRateCeiling = 0.02;

// Per-year rate fan-out. Small counts at N=300 make anything tighter a
// coin flip; anything looser would not catch a budget keyed to a window
// constant.
constexpr double kMaxYearlyRateSpread = 4.0;

// Deflated fraud-amount spread across years (U-6 class F: fraud
// lognormals are CPI-realized, so calibration-dollar means are flat).
constexpr double kMaxDeflatedAmountSpread = 2.5;

int g_failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

[[nodiscard]] bool isCardPurchase(const Txn &t) {
  return t.session.channel.value ==
         channels::tag(channels::Legit::cardPurchase).value;
}

[[nodiscard]] bool isMerchantPos(const Txn &t) {
  return t.session.channel.value ==
         channels::tag(channels::Legit::merchant).value;
}

// The card VIEW is exactly what the exporter folds into
// cf_Payment_Transaction (card_fraud/schema.hpp).
[[nodiscard]] bool inCardView(const Txn &t) {
  return isCardPurchase(t) || isMerchantPos(t);
}

[[nodiscard]] int yearOf(const Txn &t) {
  return pl::time::toCalendarDate(pl::time::fromEpochSeconds(t.timestamp))
      .year;
}

struct YearCell {
  std::size_t rows = 0;
  std::size_t fraud = 0;
  double fraudAmount = 0.0;

  [[nodiscard]] double rate() const {
    return rows == 0 ? 0.0
                     : static_cast<double>(fraud) / static_cast<double>(rows);
  }
  [[nodiscard]] double meanFraudAmount() const {
    return fraud == 0 ? 0.0 : fraudAmount / static_cast<double>(fraud);
  }
};

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  pltest::announceLeg("card prevalence leg (1991, 1461d = 4 whole years)");
  LegOptions opt;
  opt.seed = kSeed;
  opt.window.start = pl::time::makeTime(pl::time::CalendarDate{1991, 1, 1});
  opt.window.days = kDays;
  opt.population = kPopulation;
  opt.withBaseRoutines = true;
  opt.withFamily = true;
  const auto leg = pltest::runLeg(pools, opt);
  pltest::printLeg("card-prevalence", leg);

  std::map<int, YearCell> byYear;
  std::map<pl::fraud::FraudType, std::size_t> byTypology;
  std::map<pl::entity::Key, std::size_t> episodeByCard;

  std::size_t viewRows = 0;
  std::size_t viewFraud = 0;
  std::size_t cardPurchaseFraud = 0;
  std::size_t merchantPosFraud = 0;
  double fraudAmountTotal = 0.0;
  double legitAmountTotal = 0.0;

  for (const auto &t : leg.rows) {
    if (!inCardView(t)) {
      continue;
    }
    ++viewRows;
    auto &cell = byYear[yearOf(t)];
    ++cell.rows;

    if (t.fraud.flag == 0) {
      legitAmountTotal += t.amount;
      continue;
    }
    ++viewFraud;
    ++cell.fraud;
    cell.fraudAmount += t.amount;
    fraudAmountTotal += t.amount;
    ++byTypology[t.fraud.type];
    ++episodeByCard[t.source];
    if (isCardPurchase(t)) {
      ++cardPurchaseFraud;
    } else {
      ++merchantPosFraud;
    }
  }

  check(viewRows > 0 && viewFraud > 0,
        "the card view must carry both legitimate and fraudulent rows "
        "(rows " +
            std::to_string(viewRows) + ", fraud " + std::to_string(viewFraud) +
            ")");
  check(byYear.size() >= 3,
        "the leg must span at least three calendar years, got " +
            std::to_string(byYear.size()));
  if (g_failures != 0) {
    std::fprintf(stderr, "%d precondition(s) failed\n", g_failures);
    return 1;
  }

  const double rate =
      static_cast<double>(viewFraud) / static_cast<double>(viewRows);

  // ----------------------------------------------------- the aggregate
  std::printf("\n  CARD VIEW: %zu rows, %zu fraud, rate %.5f%%\n", viewRows,
              viewFraud, 100.0 * rate);
  std::printf("    vs TabFormer observed %.5f%% -> %.2fx (NAMED COMPARATOR, "
              "not a calibration target)\n",
              100.0 * kTabFormerRate, rate / kTabFormerRate);
  check(rate > kRateFloor && rate < kRateCeiling,
        "card-view fraud prevalence inside the plausibility band (" +
            std::to_string(rate) + " not in (" + std::to_string(kRateFloor) +
            ", " + std::to_string(kRateCeiling) + "))");

  // ------------------------------------------------------- per year
  //
  // COUNTS are printed, never gated: H4's real-consumption ramp raises
  // them across the era while the harness's own small-N liquidity drain
  // lowers them, and at N=300 the two are not separable. The RATE is
  // what the budget law promises, so the rate is what is gated.
  std::printf("\n  PER YEAR (counts printed, RATE gated — F = pL/(1-p) "
              "rides realized L)\n");
  double minRate = 1.0;
  double maxRate = 0.0;
  double minDeflated = 0.0;
  double maxDeflated = 0.0;
  bool firstDeflated = true;
  for (const auto &[year, cell] : byYear) {
    const auto scale = econ::priceScale(year);
    const auto deflated = cell.meanFraudAmount() / scale;
    std::printf("    %d  rows %6zu  fraud %4zu  rate %.5f%%  fraud mean "
                "$%8.2f  deflated $%8.2f\n",
                year, cell.rows, cell.fraud, 100.0 * cell.rate(),
                cell.meanFraudAmount(), deflated);
    if (cell.fraud == 0) {
      continue;
    }
    minRate = std::min(minRate, cell.rate());
    maxRate = std::max(maxRate, cell.rate());
    if (firstDeflated) {
      minDeflated = deflated;
      maxDeflated = deflated;
      firstDeflated = false;
    } else {
      minDeflated = std::min(minDeflated, deflated);
      maxDeflated = std::max(maxDeflated, deflated);
    }
  }

  const double rateSpread = minRate > 0.0 ? maxRate / minRate : 0.0;
  std::printf("    yearly rate spread %.2fx  <- gate: < %.2fx\n", rateSpread,
              kMaxYearlyRateSpread);
  check(minRate > 0.0 && rateSpread < kMaxYearlyRateSpread,
        "per-year card-view fraud RATE is era-stable (spread " +
            std::to_string(rateSpread) + ", gate < " +
            std::to_string(kMaxYearlyRateSpread) +
            "). A fan-out here means a fraud budget stopped riding the "
            "realized candidate count.");

  const double deflatedSpread =
      minDeflated > 0.0 ? maxDeflated / minDeflated : 0.0;
  std::printf("    deflated fraud-amount spread %.2fx  <- gate: < %.2fx "
              "(U-6 class F reaches the card rail)\n",
              deflatedSpread, kMaxDeflatedAmountSpread);
  check(minDeflated > 0.0 && deflatedSpread < kMaxDeflatedAmountSpread,
        "per-year fraud amounts are flat in CALIBRATION dollars (spread " +
            std::to_string(deflatedSpread) + ", gate < " +
            std::to_string(kMaxDeflatedAmountSpread) + ")");

  // ------------------------------------------------------- channels
  const double merchantShare = static_cast<double>(merchantPosFraud) /
                               static_cast<double>(viewFraud);
  std::printf("\n  CHANNELS  card_purchase fraud %zu, merchant-POS fraud %zu "
              "(share %.4f  <- gate: < 0.05)\n",
              cardPurchaseFraud, merchantPosFraud, merchantShare);
  check(cardPurchaseFraud > 0,
        "the unauthorized card rail and gift-card scam ride card_purchase, "
        "so that channel must carry fraud");
  check(merchantShare < 0.05,
        "the merchant (debit POS) channel carries essentially no modelled "
        "fraud (share " +
            std::to_string(merchantShare) + ")");

  // ------------------------------------------------------ typologies
  std::printf("\n  TYPOLOGIES on the card view\n");
  std::size_t unauthorized = 0;
  for (const auto &[type, count] : byTypology) {
    const auto share =
        static_cast<double>(count) / static_cast<double>(viewFraud);
    std::printf("    %-16s %5zu  %.4f\n",
                std::string{pl::fraud::fraudTypeName(type)}.c_str(), count,
                share);
    if (type == pl::fraud::FraudType::txnFraudSolo ||
        type == pl::fraud::FraudType::txnFraudRing) {
      unauthorized += count;
    }
  }
  const double unauthorizedShare =
      static_cast<double>(unauthorized) / static_cast<double>(viewFraud);
  check(byTypology.size() >= 2,
        "the card rail must not be a typology monoculture, got " +
            std::to_string(byTypology.size()) + " type(s)");
  check(unauthorizedShare > 0.30,
        "the unauthorized family dominates the card rail (share " +
            std::to_string(unauthorizedShare) + ", gate > 0.30)");

  // --------------------------------------------------------- amounts
  const double fraudMean =
      fraudAmountTotal / static_cast<double>(viewFraud);
  const double legitMean =
      legitAmountTotal / static_cast<double>(viewRows - viewFraud);
  std::printf("\n  AMOUNTS  fraud mean $%.2f vs legit mean $%.2f (%.2fx)  "
              "<- gate: fraud > legit\n",
              fraudMean, legitMean, fraudMean / legitMean);
  check(fraudMean > legitMean,
        "fraud tickets are larger than legitimate card tickets (fraud $" +
            std::to_string(fraudMean) + " vs legit $" +
            std::to_string(legitMean) + ")");

  // -------------------------------------------------------- episodes
  std::size_t maxEpisode = 0;
  for (const auto &[key, count] : episodeByCard) {
    (void)key;
    maxEpisode = std::max(maxEpisode, count);
  }
  const double meanEpisode = static_cast<double>(viewFraud) /
                             static_cast<double>(episodeByCard.size());
  std::printf("\n  EPISODES  %zu compromised cards, mean %.2f fraud rows "
              "each, max %zu  <- gate: mean in (1,20), max <= 200\n",
              episodeByCard.size(), meanEpisode, maxEpisode);
  check(meanEpisode > 1.0 && meanEpisode < 20.0,
        "an unauthorized episode is a handful of rows, not a career (mean " +
            std::to_string(meanEpisode) + ")");
  check(maxEpisode <= 200,
        "no single card absorbs the corpus (max episode " +
            std::to_string(maxEpisode) + ")");

  if (g_failures != 0) {
    std::fprintf(stderr, "\n%d prevalence gate(s) failed\n", g_failures);
    return 1;
  }
  std::printf("\ntest_card_prevalence: card-view prevalence is measured and "
              "era-stable (rate %.5f%%, yearly spread %.2fx)\n",
              100.0 * rate, rateSpread);
  return 0;
}
