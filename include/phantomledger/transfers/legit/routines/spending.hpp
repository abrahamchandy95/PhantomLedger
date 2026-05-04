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

class SpendingRoutine {
public:
  struct Execution {
    random::Rng &rng;
    const transactions::Factory &txf;
  };

  struct AccountSource {
    const entity::account::Lookup &lookup;
    const entity::account::Registry &registry;
  };

  struct CensusSource {
    const blueprints::LegitBuildPlan &blueprint;
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

  [[nodiscard]] std::vector<transactions::Transaction>
  run(Execution execution, const CensusSource &census, PayeeDirectory payees,
      ObligationSource obligations, LedgerReplay replay) const;

private:
  SpendingHabits habits_{};
  RunPlanning planning_{};
  DayPattern day_{};
  DynamicsProfile dynamics_{};
  EmissionProfile emission_{};
};

[[nodiscard]] std::vector<transactions::Transaction> generateDayToDayTxns(
    SpendingRoutine::Execution execution,
    const SpendingRoutine::CensusSource &census,
    SpendingRoutine::PayeeDirectory payees = SpendingRoutine::PayeeDirectory{},
    SpendingRoutine::ObligationSource obligations =
        SpendingRoutine::ObligationSource{},
    SpendingRoutine::LedgerReplay replay = SpendingRoutine::LedgerReplay{});

} // namespace PhantomLedger::transfers::legit::routines::spending
