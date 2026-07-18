#pragma once

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace PhantomLedger::transfers::fraud::typologies::amounts {

// Research-calibrated amount samplers for unauthorized (third-party)
// transaction fraud and victim-authorized scams. Shared by the
// card-compromise, account-takeover and gift-card-scam rails; the
// ring-run transaction-fraud step (txn_fraud_ring) is expected to draw
// from the same functions.
//
// Design rules (all samplers):
//  * Draw ONLY from the caller's Rng. No hidden state, no globals.
//  * Use the house Box-Muller lognormal (lognormalByMedian), never
//    std::lognormal_distribution, so cross-toolchain digest drift stays
//    limited to documented floating-point contraction rather than
//    stdlib distribution implementation differences.
//  * CLAMP tails instead of rejection-sampling. Every call consumes a
//    fixed draw pattern (exactly 2 uniforms), which keeps output
//    independent of thread/chunk scheduling once fraud is re-keyed
//    through RngFactory (S9).
//  * Round to cents.

namespace detail {

[[nodiscard]] inline double cents(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

} // namespace detail

/// Card-testing micro-charge: $0.50-$5.00, ~40% of mass on "round"
/// anchor amounts {$0.50, $1, $1, $2, $5} ($1 double-weighted).
/// Round, tiny, repeated amounts are the card-testing signature.
///
/// Sources (fetched 2026-07):
///  * Chargeflow, "Card Testing Fraud" (2026): attacks present as spikes
///    of authorizations between $0.50 and $5, plus zero-dollar
///    authorization-only requests, in rapid succession.
///    https://www.chargeflow.io/chargebacks-101/card-testing
///  * Fraudlogix, card-testing glossary (2025): test transactions are
///    typically $1 or less, or authorization holds that never complete.
///    https://www.fraudlogix.com/glossary/what-is-card-testing-and-how-to-prevent-it/
///
/// Zero-dollar auth-only probes are real but deliberately NOT emitted:
/// this is a settlement-only corpus and clearing::Ledger::decide()
/// rejects amount <= 0 (RejectReason::invalid), so a $0 event would be
/// silently dropped in replay. The $0.50 floor is the smallest
/// settleable probe.
[[nodiscard]] inline double cardTestCharge(random::Rng &rng) {
  static constexpr std::array<double, 5> kAnchors{0.50, 1.00, 1.00, 2.00, 5.00};
  // Fixed draw pattern: exactly 2 uniforms per call on both branches.
  const bool onAnchor = rng.coin(0.40);
  const double u = rng.nextDouble();
  if (onAnchor) {
    auto idx =
        static_cast<std::size_t>(u * static_cast<double>(kAnchors.size()));
    idx = std::min(idx, kAnchors.size() - 1);
    return kAnchors[idx];
  }
  return detail::cents(0.50 + 4.50 * u);
}

/// Post-validation fraudulent card spend at ordinary billers.
/// Lognormal by median: median $79, sigma 1.2 (analytic mean ~= $162,
/// clamps pull it slightly lower), clamped to [$1, $5,000].
///
/// Sources (fetched 2026-07):
///  * Security.org card-fraud reports: median fraudulent charge $62
///    (2021), $79 (2022), $100 (2024-2026). We pin the $79 figure as
///    representative of the corpus era (simulation window ends 2019;
///    the older median is the better anchor for pre-2020 spend).
///    https://www.security.org/digital-safety/credit-card-fraud-report/
///  * Chargeflow (above): validated cards escalate from micro-tests to
///    larger purchases within hours to days.
[[nodiscard]] inline double cardFraudSpend(random::Rng &rng) {
  const double raw =
      probability::distributions::lognormalByMedian(rng, 79.0, 1.2);
  return detail::cents(std::clamp(raw, 1.0, 5000.0));
}

/// Account-takeover drain over bank rails (p2p to a drop account).
/// Median-weighted fat tail: lognormal median $180, sigma 1.5
/// (analytic mean ~= $554; ~0.4% of draws exceed $10,000), clamped to
/// [$10, $85,000].
///
/// Sources (fetched 2026-07):
///  * Security.org, "Account Takeover Incidents are Rising" (annual
///    report): median victim loss $180; observed extreme $85,000.
///    https://www.security.org/digital-safety/account-takeover-annual-report/
///  * Sift Q4 2024 fraud index: average direct ecommerce ATO loss
///    $442 per account, with a long tail into five figures (mean far
///    above median = fat right tail).
///    https://securityboulevard.com/2026/05/account-takeover-statistics-2026-why-ecommerce-and-media-companies-cannot-wait/
///  * Javelin 2025 / Federal Reserve reporting: ~$16B US ATO losses in
///    2024; ATO reports up more than 36% year over year (supports ATO
///    as a first-class, growing fraud family on bank rails).
///    https://deepstrike.io/blog/account-takeover-statistics
[[nodiscard]] inline double atoDrainAmount(random::Rng &rng) {
  const double raw =
      probability::distributions::lognormalByMedian(rng, 180.0, 1.5);
  return detail::cents(std::clamp(raw, 10.0, 85000.0));
}

/// Gift-card purchase in a victim-AUTHORIZED impostor scam: the
/// scammer keeps the victim on the phone and directs them to buy
/// gift cards at retail, usually at the maximum denomination the rack
/// allows. 75% of mass on retail denominations {$100, $200, $500}
/// with $500 triple-weighted ("buy the biggest card they have");
/// remainder uniform $50-$500 snapped to $10 steps (racks sell $10
/// increments). Round amounts, one-or-two merchants, minutes apart —
/// the reportable scam signature.
///
/// Sources (named 2026-07; RECALLED, not fetched — the owner's
/// retrieval pass verifies, per docs/fraud_model_audit.md F-4):
///  * FTC Data Spotlight, "Scammers prefer gift cards" family
///    (2021-2023): gift cards the most-reported scam payment method
///    for years; victims directed to buy multiple max-denomination
///    cards; Target/Apple/Google Play the top-named brands; reported
///    gift-card scam losses ~$217M in 2023; typical per-card demand
///    $100-$500 [Likely on vintages].
///  * Major retailer per-card caps commonly $500 (Target/Apple racks)
///    [Likely].
///  * Recovery is rare once codes are read out — modeled as NO
///    reimbursement, in deliberate contrast to the unauthorized card
///    rail (Reg Z zero-liability) which is mostly reimbursed.
[[nodiscard]] inline double giftCardScamAmount(random::Rng &rng) {
  static constexpr std::array<double, 5> kDenoms{100.0, 200.0, 500.0, 500.0,
                                                 500.0};
  // Fixed draw pattern: exactly 2 uniforms per call on both branches.
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

} // namespace PhantomLedger::transfers::fraud::typologies::amounts
