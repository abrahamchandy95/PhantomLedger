#pragma once
/*
 * Installment-loan behavior terms.
 *
 * Mortgage, auto loan, and student loan products all share the same
 * delinquency-behavior model: late payments, missed cycles, partial
 * payments, cure payments, and delinquency clustering. Each concrete
 * product type stores its own principal/rate/amortization schedule
 * but delegates the miss/partial/cure knobs to this type.
 *
 * The obligations emitter looks up `InstallmentTerms` via the portfolio
 * when it sees an ObligationEvent tagged with an installment product
 * type. Non-loan products produce events that the emitter handles with
 * light timestamp jitter only.
 */

#include <cstdint>

namespace PhantomLedger::entity::product {

/// Probabilities and ranges that control realized-payment behavior
/// for a single installment-loan account.
///
/// All probabilities are in [0, 1]; callers clamp effective values to
/// 0.95 to preserve a non-trivial tail. `partialMinFrac` and
/// `partialMaxFrac` both live in (0, 1] and the max must be >= min.
struct InstallmentTerms {
  /// Baseline probability that a payment lands after the due date.
  double lateP = 0.0;

  /// Inclusive range of days a late payment is offset by. When
  /// `lateDaysMax == 0` the payment still posts on the due date even
  /// if `lateP` fires.
  std::int32_t lateDaysMin = 0;
  std::int32_t lateDaysMax = 0;

  /// Probability that a scheduled cycle produces no payment at all.
  /// Missed amounts carry forward into the next cycle's total due.
  double missP = 0.0;

  /// Probability that a cycle pays some but not all of the amount due.
  double partialP = 0.0;

  /// Probability that a delinquent account catches up and clears all
  /// arrears in one shot. The emitter scales this up mildly once the
  /// delinquent streak exceeds one cycle.
  double cureP = 0.0;

  /// Fraction of total due paid when a partial payment fires. The
  /// emitter draws uniformly from [min, max].
  double partialMinFrac = 0.0;
  double partialMaxFrac = 0.0;

  /// Multiplier applied to lateP / missP / partialP for each prior
  /// delinquent cycle (capped at two applications). Values > 1.0
  /// make delinquency sticky; 1.0 disables clustering.
  double clusterMult = 1.0;
};

} // namespace PhantomLedger::entity::product
