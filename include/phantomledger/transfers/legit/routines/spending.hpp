#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/routines/spending/behavior.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::entity::product {
class PortfolioRegistry;
} // namespace PhantomLedger::entity::product

namespace PhantomLedger::transfers::legit::routines::spending {

struct SpendingRunRequest {
  random::Rng *rng = nullptr;
  const transactions::Factory *txf = nullptr;

  const entity::account::Lookup *accountsLookup = nullptr;
  const entity::merchant::Catalog *merchants = nullptr;
  const entity::product::PortfolioRegistry *portfolios = nullptr;
  const entity::card::Registry *creditCards = nullptr;

  const blueprints::LegitBuildPlan &plan;
  const entity::account::Registry &registry;
};

struct SpendingLedgerSeed {
  std::span<const transactions::Transaction> baseTxns{};
  clearing::Ledger *screenBook = nullptr;
  bool baseTxnsSorted = false;
};

class SpendingRoutine {
public:
  SpendingRoutine() = default;

  SpendingRoutine &habits(SpendingHabits value) noexcept;
  SpendingRoutine &planning(RunPlanning value) noexcept;
  SpendingRoutine &dayPattern(DayPattern value) noexcept;
  SpendingRoutine &dynamics(DynamicsProfile value) noexcept;
  SpendingRoutine &emission(EmissionProfile value) noexcept;

  [[nodiscard]] std::vector<transactions::Transaction>
  run(const SpendingRunRequest &run, SpendingLedgerSeed seed = {}) const;

private:
  SpendingHabits habits_{};
  RunPlanning planning_{};
  DayPattern day_{};
  DynamicsProfile dynamics_{};
  EmissionProfile emission_{};
};

[[nodiscard]] std::vector<transactions::Transaction>
generateDayToDayTxns(const SpendingRunRequest &run,
                     SpendingLedgerSeed seed = {});

} // namespace PhantomLedger::transfers::legit::routines::spending
