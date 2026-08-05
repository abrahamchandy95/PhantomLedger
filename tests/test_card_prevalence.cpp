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
//   AMOUNT DIRECTION. Fraud tickets are larger than legitimate ones.
//
//   EPISODE SIZE. Rows per compromised card, bounded. An unauthorized
//   episode is a handful of transactions, not a career.
//
// WHAT IS PRINTED AND DELIBERATELY NOT GATED:
//
//   PER-YEAR CARD-VIEW ROW COUNTS. H4's real-consumption ramp raises
//   them across the era while the harness's own small-N liquidity drain
//   lowers them; at N=300 the two are not separable.
//
//   THE DEFLATED FRAUD-AMOUNT SPREAD. This WAS gated at < 2.50x and is
//   now printed, with the decomposition that explains why. See THE
//   DEFLATED-SPREAD RECLASSIFICATION below — it was mis-specified, not
//   mis-calibrated.
//
// The aggregate rate carries a NAMED COMPARATOR rather than a tight
// band: the Federal Reserve Payments Study observed 71,378,082
// fraudulent card payments in 101,735,053,260 US general-purpose card
// payments in 2016 (0.070161% BY NUMBER; credit alone 0.117039%).
// PhantomLedger is a modelled world, not a replica of any issuer's
// book, so the gate is a wide plausibility band and the ratio is
// PRINTED for the record. The COUNT basis is load-bearing — the same
// Fed table's VALUE column reads 13.458 bp, 1.9182x higher, and three
// widely-published VALUE rates sit within 10% of a count rate.
//
// THE DEFLATED-SPREAD RECLASSIFICATION (join-cohort round, authority
// U-13 ADDENDUM). The sub-gate used to read: per-year fraud means
// DEFLATED by priceScale(year) are flat within 2.50x, captioned "U-6
// class F reaches the card rail". The join-cohort world shape re-rolled
// the corpus and it went red at 2.69x, which sent someone to read the
// amount model — and the gate was measuring something the model
// explicitly does not promise. TWO INDEPENDENT DEFECTS:
//
//   (1) MIS-SPECIFIED. `unauthorized.cpp` is explicit: priceScale
//       applies to "the continuous samplers (cardFraudSpend,
//       atoDrainAmount)"; "the denomination samplers (cardTestCharge,
//       giftCardScamAmount) stay FIXED-NOMINAL — owner-approved lattice
//       CHOICE, authority U-6". The card view therefore mixes THREE
//       amount families: card-test probes ($0.50-$5, fixed-nominal),
//       gift-card scam rows ($50-$500 with 75% mass on {100,200,500,
//       500,500}, fixed-nominal) and card spends (lognormal median $79
//       sigma 1.2, CPI-scaled). Deflating the COMBINED mean by
//       priceScale and asserting flatness asserts that the two
//       fixed-nominal lattices are CPI-realized, which is the opposite
//       of the U-6 CHOICE. test_econ_wiring already knew this — its
//       `isScaledFraudRow()` EXCLUDES the whole card-purchase rail "whose
//       card-test / gift-card lattices are fixed-nominal by the
//       owner-approved U-6 CHOICE" — so the two gates contradicted each
//       other and this one was wrong.
//
//       The mixture WEIGHT is what moves the statistic, and it is a
//       small-count draw: a $500 gift-card row is ~5x a spend row and
//       ~250x a probe row, and only ~20 gift-card rows exist across four
//       years. One year drawing eight and another drawing one moves the
//       per-year mean by more than the CPI ever does.
//
//   (2) UNDER-POWERED even on a pure CPI-scaled population. The per-year
//       mean of 42-92 draws from lognormal(median 79, sigma 1.2) has
//       CV = sqrt(e^(sigma^2)-1)/sqrt(n) = 1.79/sqrt(n), i.e. 19-28%.
//       A 2.50x max/min envelope over four such means is INSIDE normal
//       sampling variation, so the old 1.79x reading was luck, not
//       evidence.
//
// The measurement is retained and DECOMPOSED (lattice vs CPI-scaled,
// nominal and deflated, per year) so the replacement can be sized from
// data rather than from a guess. REGISTERED: the class-F claim for the
// card rail wants the instrument that already works for the ring rail in
// test_econ_wiring — a TWO-ERA leg (1991 vs 2019) comparing the
// non-lattice card fraud mean, where the cross-era division cancels the
// mixture instead of riding it. That is a new gate and its own round.
// The class-F LAW itself stays pinned meanwhile by test_econ_scale (the
// scale), test_fraud_amounts (the samplers) and test_econ_wiring (the
// ring rail's cross-era ratio).
//
// WORLD SHAPE (join-cohort round): the leg carries the PRODUCTION join
// cohort — 15 joiners at 300 people over 1,461 days from 1991, by the
// BEA sizing in synth/personas/join.hpp, MEASURED at 15 on the first run
// (so no joiner drew day 0) — instead of the joinerless harness world
// this suite was calibrated against (window_leg_support.hpp, WORLD
// SHAPE). A joiner's dob AND persona timeline anchor at their JOIN date,
// so their ages, transitions and lifespans move; the lanes that read
// them consume the shared stream a different NUMBER of times once a
// transition or death crosses a window boundary, and the corpus
// downstream of the first such read is a FRESH REALIZATION rather than a
// 5% edit. MEASURED before and after, with the verdict on each band:
//
//   band                            pre-flip -> post-flip   verdict
//   aggregate rate (0.02%, 2%)      0.13665% -> 0.13786%    UNCHANGED
//   >= 3 calendar years             4 -> 4                  UNCHANGED
//   yearly RATE spread < 4.00x      1.12x -> 1.85x          UNCHANGED
//   merchant-POS share < 0.05       0.0000 -> 0.0000        UNCHANGED
//   card_purchase fraud > 0         256 -> 253              UNCHANGED
//   >= 2 typologies                 2 -> 2                  UNCHANGED
//   unauthorized share > 0.30       0.9062 -> 0.9209        UNCHANGED
//   fraud mean > legit mean         2.21x -> 2.08x          UNCHANGED
//   episode mean (1,20), max <= 200 7.11/15 -> 7.23/14      UNCHANGED
//   deflated spread < 2.50x         1.79x -> 2.69x          RECLASSIFIED
//                                                           (see above)
//
// So NO BAND WAS WIDENED OR RE-CENTRED by the world-shape change. The
// one that moved was removed because it was measuring the wrong
// quantity, and the yearly RATE spread — the gate that actually carries
// the budget law — absorbed the re-roll with 1.85x against a 4.00x
// bound. The world shape is PINNED below.
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

// US general-purpose card-payment fraud prevalence BY NUMBER OF
// TRANSACTIONS. A NAMED COMPARATOR, not a calibration target.
//
// Board of Governors of the Federal Reserve System, "Changes in U.S.
// Payments Fraud from 2012 to 2016: Evidence from the Federal Reserve
// Payments Study", October 2018 — Data Tables workbook
// (frps_fraud_data.xls), Table B.5.A (numerator), Table B.6.A
// (denominator), Table B.7.A and text Table 10 (published rate).
// Source survey: NPIPS, the card-network survey.
// https://www.federalreserve.gov/paymentsystems/files/frps_fraud_data.xls
// DATA YEAR 2016. ACCESSED 2026-08-05. CLASS: CITED.
//
//   71,378,082 fraudulent / 101,735,053,260 card payments = 7.016076 bp
//   published 7.016075570349938 bp, relative error 2.6e-09
//   credit + debit close exactly:  40,105,286 +  31,272,796 = numerator
//                          34,266,518,107 + 67,468,535,153 = denominator
//
// COUNT BASIS. **NOT the 13.458 bp figure in the Value column of the
// SAME TABLE** — that is dollars lost per dollar spent and is 1.9182x
// this one, being 7.016 bp x ($104.83 mean fraudulent ticket / $54.65
// mean overall ticket). Comparing a count rate against value basis
// points is the one substitution this gate exists to forbid.
//
// THREE PUBLISHED VALUE RATES SIT WITHIN 10% OF A COUNT RATE AND WOULD
// LEAVE THIS GATE GREEN WHILE BEING THE WRONG BASIS: Nilson worldwide
// 2024 is 6.43 cents per $100 = 0.064349% BY VALUE (9% from the anchor
// below); Nilson US 2023 is 0.11009% BY VALUE and Nilson US 2024
// 0.1024% BY VALUE. Value and count rates coincide here only because
// the mean fraudulent ticket happens to sit near the mean legitimate
// one. There is no law making them equal and the gap moves with the
// CNP mix.
//
// SCOPE: US general-purpose networks (credit + non-prepaid debit +
// prepaid debit). ATM withdrawals EXCLUDED. Private-label/store cards
// EXCLUDED. Purchases AND bill payments. Card-present and
// card-not-present COMBINED. Third-party (unauthorized) fraud only,
// cleared and settled, BEFORE chargebacks, returns or recoveries.
//
// WHY ALL-CARDS AND NOT CREDIT-ONLY: the card view is credit AND debit
// ('C'/'D' + renderAccountKey, card-churn-2026-07), so the blend is the
// scope match. The credit-only row of the same table is
// 40,105,286 / 34,266,518,107 = 0.117039% and is PRINTED alongside — it
// lands within 0.25% of the retired prior anchor, which is a
// COINCIDENCE and not a reason to prefer it.
//
// 2016 IS THE LAST BY-NUMBER CARD-FRAUD YEAR THE FRPS EVER PUBLISHED —
// there is no newer release, and everything since (Regulation II, KC
// Fed briefings) is either VALUE basis or debit-only with no published
// numerator to divide. THE GATE LEG RUNS 1991-1994; no by-number rate
// exists for any year in that window from any source. That era gap is
// tolerable ONLY because this is a printed comparator inside a 28x-wide
// plausibility band. IF ANYONE TIGHTENS THE BAND, THE ANCHOR MUST BE
// DATED FIRST (the series has 2012/2015/2016 points) OR THE LEG RE-SITED.
constexpr double kFrpsCardFraudRateByNumber = 0.00070161;   // 7.0161 bp
constexpr double kFrpsCreditFraudRateByNumber = 0.00117039; // 11.7039 bp

// Wide plausibility band around card-fraud prevalence: 0.02% to 2%.
// ABSOLUTE, and DELIBERATELY NOT RE-DERIVED FROM THE ANCHOR ABOVE —
// under it the band reads [0.285x, 28.5x]. The published record itself
// disagrees by: 1.195x between the Fed's OWN two surveys on the same
// instrument in the same year (NPIPS 11.697 vs DFIPS 9.787 bp, 2015
// credit); 1.19x between Reg II Table 10 and its own components
// re-weighted; 2.52x credit vs debit; 2.3x 2016->2023 era drift; 5.97x
// card-present vs card-not-present; 32x prepaid vs non-prepaid. Those
// compound multiplicatively, so ~6x is the tightest DEFENSIBLE envelope
// and anything below that asserts precision nobody has.
constexpr double kRateFloor = 0.0002;
constexpr double kRateCeiling = 0.02;

// Per-year rate fan-out. Small counts at N=300 make anything tighter a
// coin flip; anything looser would not catch a budget keyed to a window
// constant.
constexpr double kMaxYearlyRateSpread = 4.0;

// cardFraudSpend's upper clamp in CALIBRATION dollars (amounts.hpp): the
// realized ceiling is this x priceScale(event year). Used for a PRINTED
// class-F diagnostic — an unscaled clamp would show a ratio above 1 in
// the early era (5000 vs 5000 x 0.53). Gift-card rows cap at $500, well
// under 5000 x priceScale for every in-era year, so one ceiling covers
// the whole view.
constexpr double kCardSpendClampCalibration = 5000.0;

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

// The U-6 FIXED-NOMINAL denomination lattices, as far as a Transaction
// can resolve them. giftCardScamAmount is identifiable by typology.
// cardTestCharge is NOT: probes carry the same txnFraudSolo/txnFraudRing
// type as the CPI-scaled spends they precede, so they stay in the
// "scaled" bucket below and drag its per-year mean by a year-varying
// amount. That residue is exactly why the decomposition is PRINTED and
// not gated — see THE DEFLATED-SPREAD RECLASSIFICATION in the file
// comment.
[[nodiscard]] bool isFixedNominalLattice(const Txn &t) {
  return t.fraud.type == pl::fraud::FraudType::scamGiftCard;
}

[[nodiscard]] int yearOf(const Txn &t) {
  return pl::time::toCalendarDate(pl::time::fromEpochSeconds(t.timestamp)).year;
}

struct YearCell {
  std::size_t rows = 0;
  std::size_t fraud = 0;
  double fraudAmount = 0.0;

  // The U-6 lattice split (see isFixedNominalLattice).
  std::size_t latticeFraud = 0;
  double latticeAmount = 0.0;
  std::size_t scaledFraud = 0;
  double scaledAmount = 0.0;

  // Largest fraud amount seen, for the class-F clamp diagnostic.
  double maxFraudAmount = 0.0;

  [[nodiscard]] double rate() const {
    return rows == 0 ? 0.0
                     : static_cast<double>(fraud) / static_cast<double>(rows);
  }
  [[nodiscard]] double meanFraudAmount() const {
    return fraud == 0 ? 0.0 : fraudAmount / static_cast<double>(fraud);
  }
  [[nodiscard]] double meanLatticeAmount() const {
    return latticeFraud == 0
               ? 0.0
               : latticeAmount / static_cast<double>(latticeFraud);
  }
  [[nodiscard]] double meanScaledAmount() const {
    return scaledFraud == 0 ? 0.0
                            : scaledAmount / static_cast<double>(scaledFraud);
  }
};

// max/min over a series, 0.0 when the series has a non-positive member
// (an absent year cannot make a spread).
[[nodiscard]] double spreadOf(const std::vector<double> &values) {
  if (values.empty()) {
    return 0.0;
  }
  double lo = values.front();
  double hi = values.front();
  for (const double v : values) {
    if (v <= 0.0) {
      return 0.0;
    }
    lo = std::min(lo, v);
    hi = std::max(hi, v);
  }
  return lo > 0.0 ? hi / lo : 0.0;
}

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

  // WORLD SHAPE, asserted before anything is measured: prevalence is a
  // claim about the shipped corpus, so it has to be measured on the
  // shipped population. A zero here means the leg rebuilt the
  // pre-H3-3c-ii joinerless world and every band below is describing
  // something else.
  check(leg.joiners > 0,
        "the leg carries the production join cohort (joiners " +
            std::to_string(leg.joiners) + ")");

  std::map<int, YearCell> byYear;
  std::map<pl::fraud::FraudType, std::size_t> byTypology;
  std::map<pl::entity::Key, std::size_t> episodeByCard;

  std::size_t viewRows = 0;
  std::size_t viewFraud = 0;
  std::size_t cardPurchaseFraud = 0;
  std::size_t merchantPosFraud = 0;
  std::size_t unauthorizedCredit = 0;
  std::size_t unauthorizedDebit = 0;
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
    cell.maxFraudAmount = std::max(cell.maxFraudAmount, t.amount);
    if (isFixedNominalLattice(t)) {
      ++cell.latticeFraud;
      cell.latticeAmount += t.amount;
    } else {
      ++cell.scaledFraud;
      cell.scaledAmount += t.amount;
    }
    fraudAmountTotal += t.amount;
    ++byTypology[t.fraud.type];
    ++episodeByCard[t.source];
    if (isCardPurchase(t)) {
      ++cardPurchaseFraud;
      if (t.fraud.type == pl::fraud::FraudType::txnFraudSolo ||
          t.fraud.type == pl::fraud::FraudType::txnFraudRing) {
        if (t.source.role == pl::entity::Role::card) {
          ++unauthorizedCredit;
        } else if (t.source.role == pl::entity::Role::account) {
          ++unauthorizedDebit;
        }
      }
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
  std::printf("\n  WORLD SHAPE: join cohort %llu of %d people (production "
              "sizing)\n",
              static_cast<unsigned long long>(leg.joiners), kPopulation);
  std::printf("  CARD VIEW: %zu rows, %zu fraud, rate %.5f%%\n", viewRows,
              viewFraud, 100.0 * rate);
  std::printf("    vs FRPS 2016 all general-purpose cards %.5f%% BY NUMBER "
              "-> %.2fx (NAMED COMPARATOR, not a calibration target)\n",
              100.0 * kFrpsCardFraudRateByNumber,
              rate / kFrpsCardFraudRateByNumber);
  std::printf("    vs FRPS 2016 credit only              %.5f%% BY NUMBER "
              "-> %.2fx (PRINTED; the view is credit AND debit)\n",
              100.0 * kFrpsCreditFraudRateByNumber,
              rate / kFrpsCreditFraudRateByNumber);
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
  std::vector<double> deflatedAll;
  std::vector<double> deflatedScaled;
  std::vector<double> nominalAll;
  double maxClampRatio = 0.0;
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
    deflatedAll.push_back(deflated);
    nominalAll.push_back(cell.meanFraudAmount());
    if (cell.scaledFraud > 0) {
      deflatedScaled.push_back(cell.meanScaledAmount() / scale);
    }
    maxClampRatio =
        std::max(maxClampRatio,
                 cell.maxFraudAmount / (kCardSpendClampCalibration * scale));
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

  // --------------------------------- the U-6 amount decomposition
  //
  // PRINTED, NEVER GATED — see THE DEFLATED-SPREAD RECLASSIFICATION in
  // the file comment. The card view mixes two FIXED-NOMINAL denomination
  // lattices with one CPI-scaled lognormal, so a deflated-flatness claim
  // over the combined mean asserts the opposite of the U-6 CHOICE, and
  // at 42-92 draws a year the mean is 19-28% noisy besides. These lines
  // exist so the replacement two-era instrument can be sized from data.
  std::printf("\n  AMOUNT DECOMPOSITION (U-6 lattice vs CPI-scaled; PRINTED, "
              "NEVER GATED)\n");
  std::printf("    year   lattice(n/mean)      scaled(n/mean)       "
              "scaled deflated\n");
  for (const auto &[year, cell] : byYear) {
    if (cell.fraud == 0) {
      continue;
    }
    const auto scale = econ::priceScale(year);
    std::printf("    %d   %4zu / $%8.2f     %4zu / $%8.2f     $%8.2f\n", year,
                cell.latticeFraud, cell.meanLatticeAmount(), cell.scaledFraud,
                cell.meanScaledAmount(),
                cell.scaledFraud > 0 ? cell.meanScaledAmount() / scale : 0.0);
  }
  std::printf("    spreads: nominal all %.2fx | deflated all %.2fx | "
              "deflated CPI-scaled-only %.2fx\n",
              spreadOf(nominalAll), spreadOf(deflatedAll),
              spreadOf(deflatedScaled));
  std::printf("    ^ the CPI-scaled column still carries the card-test probe "
              "lattice ($0.50-$5, fixed-nominal, not resolvable from a "
              "row)\n");
  std::printf("    class-F clamp check: max fraud amount / (%.0f x "
              "priceScale(year)) = %.4f  <- an UNSCALED clamp would exceed "
              "1.0 in the early era\n",
              kCardSpendClampCalibration, maxClampRatio);

  // ------------------------------------------------------- channels
  const double merchantShare =
      static_cast<double>(merchantPosFraud) / static_cast<double>(viewFraud);
  std::printf("\n  CHANNELS  card_purchase fraud %zu, merchant-POS fraud %zu "
              "(share %.4f  <- gate: < 0.05)\n",
              cardPurchaseFraud, merchantPosFraud, merchantShare);
  check(cardPurchaseFraud > 0,
        "the modeled unauthorized debit-card rail and gift-card scam ride "
        "card_purchase, so that channel must carry fraud");
  check(merchantShare < 0.05,
        "the merchant (debit POS) channel carries essentially no modelled "
        "fraud (share " +
            std::to_string(merchantShare) + ")");

  std::printf("  INSTRUMENTS unauthorized credit %zu, debit %zu\n",
              unauthorizedCredit, unauthorizedDebit);
  check(unauthorizedDebit > 0,
        "unauthorized card rows use the modeled debit-card instrument");
  check(unauthorizedCredit == 0,
        "late-injected unauthorized rows never bypass credit-card lifecycle "
        "servicing by sourcing an already-closed modeled liability");

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
  const double fraudMean = fraudAmountTotal / static_cast<double>(viewFraud);
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
  check(maxEpisode <= 200, "no single card absorbs the corpus (max episode " +
                               std::to_string(maxEpisode) + ")");

  if (g_failures != 0) {
    std::fprintf(stderr, "\n%d prevalence gate(s) failed\n", g_failures);
    return 1;
  }
  std::printf("\ntest_card_prevalence: card-view prevalence is measured and "
              "era-stable (rate %.5f%%, yearly spread %.2fx, joiners %llu)\n",
              100.0 * rate, rateSpread,
              static_cast<unsigned long long>(leg.joiners));
  return 0;
}
