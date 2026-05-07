#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/routines/spending/behavior.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::entity::product {
class PortfolioRegistry;
} // namespace PhantomLedger::entity::product

namespace PhantomLedger::transfers::legit::routines::spending {

class SpendingRoutine {
public:
  struct Execution {
    random::Rng &rng;
    const transactions::Factory &txf;
    std::uint64_t seed = 0;
  };

  struct AccountSource {
    const entity::account::Lookup &lookup;
    const entity::account::Registry &registry;
  };

  struct CensusSource {
    const blueprints::LegitBlueprint &blueprint;
    AccountSource accounts;
  };

  struct PayeeDirectory {
    const entity::merchant::Catalog *merchants = nullptr;
    const entity::card::Registry *creditCards = nullptr;
  };

  struct ObligationSource {
    const entity::product::PortfolioRegistry *portfolios = nullptr;
  };

  struct LedgerReplay {
    std::span<const transactions::Transaction> baseTxns{};
    clearing::Ledger *screenBook = nullptr;
    bool baseTxnsSorted = false;
  };

  SpendingRoutine() = default;

  SpendingRoutine &habits(SpendingHabits value) noexcept;
  SpendingRoutine &planning(RunPlanning value) noexcept;
  SpendingRoutine &dayPattern(DayPattern value) noexcept;
  SpendingRoutine &dynamics(DynamicsProfile value) noexcept;
  SpendingRoutine &emission(EmissionProfile value) noexcept;

  [[nodiscard]] ::PhantomLedger::spending::market::Market
  prepareMarket(const CensusSource &census, PayeeDirectory payees,
                std::span<const transactions::Transaction> baseTxns) const;

  [[nodiscard]] static ::PhantomLedger::spending::obligations::Snapshot
  prepareObligations(const CensusSource &census, ObligationSource obligations,
                     std::span<const transactions::Transaction> baseTxns,
                     bool baseTxnsSorted);

  [[nodiscard]] std::vector<transactions::Transaction>
  run(Execution execution, ::PhantomLedger::spending::market::Market &market,
      const ::PhantomLedger::spending::obligations::Snapshot &obligations,
      clearing::Ledger *screenBook = nullptr) const;

private:
  SpendingHabits habits_{};
  RunPlanning planning_{};
  DayPattern day_{};
  DynamicsProfile dynamics_{};
  EmissionProfile emission_{};
};

} // namespace PhantomLedger::transfers::legit::routines::spending
