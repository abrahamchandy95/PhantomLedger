#pragma once
/*
 * Obligation event emitter.
 *
 * Converts ObligationEvents from the product portfolio into concrete
 * Transactions. This is the bridge between the pure product layer
 * (no infra, no fraud flags) and the transaction pipeline (with
 * device/IP routing and balance constraints).
 *
 * Installment-loan behavior modeled here:
 *   - occasional late payments
 *   - missed monthly payments
 *   - partial payments
 *   - catch-up / cure payments
 *   - delinquency clustering once an account falls behind
 *
 * Why this lives here:
 *   The product layer defines contractual due events. This transfer
 *   layer decides how those due events appear in observed
 *   transaction history.
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::obligations {

/// Minimal view of primary-account ownership. The emitter skips any
/// event whose person has no resolvable primary account (external-only
/// or unregistered).
struct Population {
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;
};

/// Convert every scheduled obligation event that falls in
/// [start, endExcl) into one or zero Transactions.
///
/// Installment-loan events (mortgage, auto loan, student loan with
/// Direction::outflow) are routed through the delinquency state
/// machine. Every other event gets light timestamp jitter applied
/// and is emitted directly.
///
/// The output is sorted by timestamp. Delinquency state is
/// internal to this call and discarded on return; to continue the
/// same state across multiple calls, splice events into the same
/// registry instance and call this once.
[[nodiscard]] std::vector<transactions::Transaction>
emit(const entity::product::PortfolioRegistry &registry, time::TimePoint start,
     time::TimePoint endExcl, const Population &population, random::Rng &rng,
     const transactions::Factory &txf);

} // namespace PhantomLedger::transfers::obligations
