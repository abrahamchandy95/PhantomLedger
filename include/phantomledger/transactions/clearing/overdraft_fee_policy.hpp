#pragma once
/*
 * OverdraftFeePolicy — decides whether a transfer triggers a
 * courtesy/linked overdraft fee.
 *
 * This is intentionally a plain function + small POD, not a class:
 * the decision is a pure function of (protection type, fee amount,
 * cash before, cash after, channel). Making it a class with virtual
 * dispatch would add allocation and an indirect call for no real
 * benefit, since the policy is shared across every internal account
 * in the simulation.
 *
 * The policy does NOT emit the LiquidityEvent or debit the ledger.
 * It returns a `FeeAssessment` that the caller turns into side
 * effects. This keeps the policy stateless and trivially testable.
 *
 * LOC accounts do NOT trigger a flat fee — they accrue interest via
 * LocAccrualTracker. NONE-protection accounts can't overdraft in the
 * first place (the funding check rejects the transfer). So the only
 * "yes" case is COURTESY or LINKED that crossed from >= 0 to < 0,
 * on a non-liquidity channel.
 */

#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/protection.hpp"

namespace PhantomLedger::clearing {

struct FeeAssessment {
  bool fires = false;
  double amount = 0.0;

  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return fires;
  }
};

/// Decide whether a transfer that just moved an account from
/// `cashBefore` to `cashAfter` should trigger an overdraft fee.
///
/// `feeAmount` is the per-account fee that was sampled during
/// bootstrap (tier-dependent, zero for the zero-fee tier).
[[nodiscard]] constexpr FeeAssessment
assessOverdraftFee(ProtectionType protection, channels::Tag channel,
                   double cashBefore, double cashAfter,
                   double feeAmount) noexcept {
  // Fees only apply under discretionary protection. LOC accrues
  // interest via the tracker; NONE can't overdraft.
  if (protection != ProtectionType::courtesy &&
      protection != ProtectionType::linked) {
    return {};
  }

  // Don't recursively fee a liquidity-event transfer (would otherwise
  // cascade: fee debits account -> account goes more negative ->
  // emit fee -> ...).
  if (channels::isLiquidity(channel)) {
    return {};
  }

  if (feeAmount <= 0.0) {
    return {};
  }

  // Only the crossing-into-negative edge triggers. Already-negative
  // accounts that dig deeper don't re-emit.
  const bool crossed = (cashBefore >= 0.0) && (cashAfter < 0.0);
  if (!crossed) {
    return {};
  }

  return FeeAssessment{.fires = true, .amount = feeAmount};
}

} // namespace PhantomLedger::clearing
