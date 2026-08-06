#pragma once

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

/*
  Fraud Amount Samplers
  Research-calibrated amounts for unauthorized (third-party) transaction fraud
  and victim-authorized scams, shared by the card-compromise, account-takeover,
  gift-card-scam and impostor-push rails. The ring-run transaction-fraud step
  (txn_fraud_ring) is expected to draw from the same functions.

  RULES THAT BIND EVERY SAMPLER HERE:
   * Draw ONLY from the caller's Rng. No hidden state, no globals.
   * Use the house Box-Muller lognormal (lognormalByMedian), never
     std::lognormal_distribution, so cross-toolchain digest drift stays limited
     to documented floating-point contraction rather than stdlib distribution
     implementation differences.
   * CLAMP tails instead of rejection-sampling. Every call consumes a FIXED
     draw pattern (exactly 2 uniforms), which keeps output independent of
     thread/chunk scheduling.
   * Round to cents (primitives::utils::roundMoney).

  CONTINUOUS vs DENOMINATION (class F, authority U-6). The continuous samplers
  (cardFraudSpend, atoDrainAmount, scamWireAmount) take the event year's CPI
  level multiplier, and the calibration-year median AND its clamps scale
  together so the tail shape is era-invariant. The denomination samplers
  (cardTestCharge, giftCardScamAmount) stay FIXED-NOMINAL: round probe amounts
  and rack denominations are physical artifacts like the $20 note, and scaling
  them destroys the signature that IS the typology.
 */

namespace PhantomLedger::transfers::fraud::typologies::amounts {

/* Card-testing micro-charge: $0.50-$5.00, ~40% of mass on "round"
 * anchor amounts {$0.50, $1, $1, $2, $5} ($1 double-weighted).
 * Round, tiny, repeated amounts are the card-testing signature.
 *
 * Sources (fetched 2026-07):
 *  * Chargeflow, "Card Testing Fraud" (2026): attacks present as spikes
 *    of authorizations between $0.50 and $5, plus zero-dollar
 *    authorization-only requests, in rapid succession.
 *    https://www.chargeflow.io/chargebacks-101/card-testing
 *  * Fraudlogix, card-testing glossary (2025): test transactions are
 *    typically $1 or less, or authorization holds that never complete.
 *    https://www.fraudlogix.com/glossary/what-is-card-testing-and-how-to-prevent-it/
 *
 * Zero-dollar auth-only probes are real but deliberately NOT emitted:
 * this is a settlement-only corpus and clearing::Ledger::decide()
 * rejects amount <= 0 (RejectReason::invalid), so a $0 event would be
 * silently dropped in replay. The $0.50 floor is the smallest
 * settleable probe. FIXED-NOMINAL (class S denomination lattice). */
[[nodiscard]] inline double cardTestCharge(random::Rng &rng) {
  static constexpr std::array<double, 5> kAnchors{0.50, 1.00, 1.00, 2.00, 5.00};
  /* Fixed draw pattern: exactly 2 uniforms per call on both branches. */
  const bool onAnchor = rng.coin(0.40);
  const double u = rng.nextDouble();
  if (onAnchor) {
    auto idx =
        static_cast<std::size_t>(u * static_cast<double>(kAnchors.size()));
    idx = std::min(idx, kAnchors.size() - 1);
    return kAnchors[idx];
  }
  return primitives::utils::roundMoney(0.50 + 4.50 * u);
}

/* Post-validation fraudulent card spend at ordinary billers.
 * Lognormal by median: median $79, sigma 1.2 (analytic mean ~= $162,
 * clamps pull it slightly lower), clamped to [$1, $5,000] — median
 * and clamps in CALIBRATION-YEAR dollars, both multiplied by
 * `priceScale` (the event year's CPI level).
 *
 * Sources (fetched 2026-07):
 *  * Security.org card-fraud reports: median fraudulent charge $62
 *    (2021), $79 (2022), $100 (2024-2026). We pin the $79 figure as
 *    representative of the corpus era (simulation window ends 2019;
 *    the older median is the better anchor for pre-2020 spend).
 *    https://www.security.org/digital-safety/credit-card-fraud-report/
 *  * Chargeflow (above): validated cards escalate from micro-tests to
 *    larger purchases within hours to days. */
[[nodiscard]] inline double cardFraudSpend(random::Rng &rng,
                                           double priceScale = 1.0) {
  const double raw =
      probability::distributions::lognormalByMedian(rng, 79.0, 1.2);
  return primitives::utils::roundMoney(
      std::clamp(raw * priceScale, 1.0 * priceScale, 5000.0 * priceScale));
}

/* Account-takeover drain over bank rails (p2p to a drop account).
 * Median-weighted fat tail: lognormal median $180, sigma 1.5
 * (analytic mean ~= $554; ~0.4% of draws exceed $10,000), clamped to
 * [$10, $85,000] — median and clamps in CALIBRATION-YEAR dollars,
 * both multiplied by `priceScale` (the event year's CPI level).
 *
 * Sources (fetched 2026-07):
 *  * Security.org, "Account Takeover Incidents are Rising" (annual
 *    report): median victim loss $180; observed extreme $85,000.
 *    https://www.security.org/digital-safety/account-takeover-annual-report/
 *  * Sift Q4 2024 fraud index: average direct ecommerce ATO loss
 *    $442 per account, with a long tail into five figures (mean far
 *    above median = fat right tail).
 *    https://securityboulevard.com/2026/05/account-takeover-statistics-2026-why-ecommerce-and-media-companies-cannot-wait/
 *  * Javelin 2025 / Federal Reserve reporting: ~$16B US ATO losses in
 *    2024; ATO reports up more than 36% year over year (supports ATO
 *    as a first-class, growing fraud family on bank rails).
 *    https://deepstrike.io/blog/account-takeover-statistics */
[[nodiscard]] inline double atoDrainAmount(random::Rng &rng,
                                           double priceScale = 1.0) {
  const double raw =
      probability::distributions::lognormalByMedian(rng, 180.0, 1.5);
  return primitives::utils::roundMoney(
      std::clamp(raw * priceScale, 10.0 * priceScale, 85000.0 * priceScale));
}

/* Gift-card purchase in a victim-AUTHORIZED impostor scam: the
 * scammer keeps the victim on the phone and directs them to buy
 * gift cards at retail, usually at the maximum denomination the rack
 * allows. 75% of mass on retail denominations {$100, $200, $500}
 * with $500 triple-weighted ("buy the biggest card they have");
 * remainder uniform $50-$500 snapped to $10 steps (racks sell $10
 * increments). Round amounts, one-or-two merchants, minutes apart —
 * the reportable scam signature. FIXED-NOMINAL (class S denomination
 * lattice).
 *
 * The victim's AGE-GRADED severity does NOT enter here, deliberately: the
 * rack caps a card at $500 whoever is buying, so the denomination cannot
 * carry it. THIS RAIL IS UNGRADED ON BOTH AXES — the card COUNT is not
 * scaled either, because grading it manufactures bursts the victim cannot
 * fund and the ledger discards. Severity applies to the impostor AMOUNT
 * alone (see scamWireAmount and injector.cpp's targetSpan comment).
 *
 * Sources (named 2026-07; RECALLED, not fetched — the owner's
 * retrieval pass verifies, per docs/fraud_model_audit.md F-4):
 *  * FTC Data Spotlight, "Scammers prefer gift cards" family
 *    (2021-2023): gift cards the most-reported scam payment method
 *    for years; victims directed to buy multiple max-denomination
 *    cards; Target/Apple/Google Play the top-named brands; reported
 *    gift-card scam losses ~$217M in 2023; typical per-card demand
 *    $100-$500 [Likely on vintages].
 *  * Major retailer per-card caps commonly $500 (Target/Apple racks)
 *    [Likely].
 *  * Recovery is rare once codes are read out — modeled as NO
 *    reimbursement, in deliberate contrast to the unauthorized card
 *    rail (Reg Z zero-liability) which is mostly reimbursed. */
[[nodiscard]] inline double giftCardScamAmount(random::Rng &rng) {
  static constexpr std::array<double, 5> kDenoms{100.0, 200.0, 500.0, 500.0,
                                                 500.0};
  /* Fixed draw pattern: exactly 2 uniforms per call on both branches. */
  const bool onDenom = rng.coin(0.75);
  const double u = rng.nextDouble();
  if (onDenom) {
    auto idx =
        static_cast<std::size_t>(u * static_cast<double>(kDenoms.size()));
    idx = std::min(idx, kDenoms.size() - 1);
    return kDenoms[idx];
  }
  return std::round((50.0 + 450.0 * u) / 10.0) * 10.0;
}

/* Victim-AUTHORIZED PUSH PAYMENT in an impostor scam: the victim wires
 * or app-pushes their own money to an account the attacker controls.
 * Lognormal median $900, sigma 1.3 (analytic mean ~= $2.1k before
 * clamping), clamped to [$50, $50,000] — an order of magnitude above
 * the card rail, which is the single most important quantitative fact
 * about authorized push fraud: far fewer cases, far larger losses.
 *
 * TWO MULTIPLIERS, both applied to the median AND both clamps so the tail
 * SHAPE is invariant and only its LEVEL moves:
 *   priceScale  the event year's CPI level (class F).
 *   severity    the victim's age-graded loss multiplier
 *               (susceptibility::scamAgeSeverity; ~0.70 in the 20s to
 *               ~2.20 at 80+, reproducing the ~3x FTC median-loss
 *               ratio across age bands).
 *
 * Sources (named 2026-07; RECALLED, not fetched — DIRECTION and ORDER
 * OF MAGNITUDE are the anchored claims, the median and sigma are
 * declared CHOICEs):
 *  * FTC Consumer Sentinel Network annual data books: imposter scams
 *    the largest report category through the corpus era; median
 *    individual reported loss in the high hundreds overall, and
 *    materially higher when the payment method is a bank transfer
 *    than when it is a card [Likely].
 *  * FTC "Protecting Older Consumers" reports to Congress: median
 *    reported loss rises monotonically across age bands, oldest band
 *    roughly 3x the youngest [Likely].
 *  * UK Finance APP-fraud reporting (the closest published per-case
 *    series for authorized push payments): per-case losses one to two
 *    orders of magnitude above card fraud [Likely].
 *
 * NO REIMBURSEMENT is emitted for this rail. In the corpus era an
 * authorized push was the victim's own instruction: US Reg E covers
 * unauthorized transfers, not authorized ones, and the UK's
 * reimbursement code postdates the window. That asymmetry against the
 * mostly-reimbursed card rail is a modeled fact, not an omission. */
[[nodiscard]] inline double scamWireAmount(random::Rng &rng,
                                           double priceScale = 1.0,
                                           double severity = 1.0) {
  const double level = priceScale * (severity > 0.0 ? severity : 1.0);
  const double raw =
      probability::distributions::lognormalByMedian(rng, 900.0, 1.3);
  return primitives::utils::roundMoney(
      std::clamp(raw * level, 50.0 * level, 50000.0 * level));
}

} // namespace PhantomLedger::transfers::fraud::typologies::amounts
