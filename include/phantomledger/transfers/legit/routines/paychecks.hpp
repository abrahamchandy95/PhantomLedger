#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::paychecks {

/// One person's deposit-splitting plan: a fraction of every salary
/// deposit landing in their primary account is forwarded to a
/// secondary account. Produced by `planSplitters`, consumed by
/// `emitSplitTransfers`.
struct Split {
  entity::Key secondary{};
  double fraction = 0.0;
};

using SplitsByPrimary = std::unordered_map<entity::Key, Split>;

/// Phase 1 — planning. Decide which persons participate in deposit
/// splitting and which secondary account receives the split. Reads
/// the plan and account topology; uses `rng` for inclusion gating
/// and the fraction draw.
[[nodiscard]] SplitsByPrimary
planSplitters(random::Rng &rng, const blueprints::LegitBlueprint &plan,
              const entity::account::Ownership &ownership,
              const entity::account::Registry &registry);

/// Phase 2 — emission. For every salary deposit whose target is a
/// primary account in `splits`, emit a follow-up self-transfer to
/// the configured secondary account.
[[nodiscard]] std::vector<transactions::Transaction>
emitSplitTransfers(random::Rng &rng, const transactions::Factory &txf,
                   const SplitsByPrimary &splits,
                   std::span<const transactions::Transaction> existingTxns);

} // namespace PhantomLedger::transfers::legit::routines::paychecks
