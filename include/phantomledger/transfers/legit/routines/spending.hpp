#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/obligations/snapshot.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/holdings/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/routines/spending/behavior.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
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

    // geo-causal-v1 (G2a): per-person home area (PersonId-1), from
    // People::homeAreas. EMPTY on the monolith reference oracle (no People
    // in addSpending's scope); the production windowed path and the test
    // world set it. Flows to population::View for card-present
    // distance-decay selection (unread until G2a step-2).
    std::span<const entity::geography::GeoAreaId> homeAreas{};

    // relocation-2026-07: the history behind the snapshot. Null ⇒ homes never
    // move.
    const entity::parties::relocation::Schedule *relocation = nullptr;
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

  struct CardLifecycleConfig {
    const entity::card::Registry *cards = nullptr;
    const ::PhantomLedger::transfers::credit_cards::LifecycleRules *rules =
        nullptr;
    entity::Key issuerAccount{};
    std::unordered_map<entity::PersonId, entity::Key> primaryAccounts{};
    ::PhantomLedger::time::Window window{};
    std::uint64_t seed = 0;

    // H3 part 3c-ii: the persona-timeline carrier (PersonId-1
    // indexed) — the CardCycleDriver truncates each card's statement
    // ladder at the owner's ACCOUNT CLOSURE minus the settlement-tail
    // guard. Filled by buildCardLifecycleConfig (passes.cpp) for BOTH
    // engines; empty stands the truncation down.
    std::span<const ::PhantomLedger::synth::personas::timeline::Timeline>
        timelines{};

    [[nodiscard]] bool active() const noexcept {
      return cards != nullptr && !cards->records.empty() && rules != nullptr &&
             window.days > 0;
    }
  };

  SpendingRoutine() = default;

  SpendingRoutine &habits(SpendingHabits value) noexcept;
  SpendingRoutine &planning(RunPlanning value) noexcept;
  SpendingRoutine &dayPattern(DayPattern value) noexcept;
  SpendingRoutine &dynamics(DynamicsProfile value) noexcept;
  SpendingRoutine &emission(EmissionProfile value) noexcept;
  SpendingRoutine &cardLifecycle(CardLifecycleConfig value);

  [[nodiscard]] ::PhantomLedger::activity::spending::market::Market
  prepareMarket(const CensusSource &census, PayeeDirectory payees,
                std::span<const transactions::Transaction> baseTxns) const;

  [[nodiscard]] static ::PhantomLedger::activity::spending::obligations::Snapshot
  prepareObligations(const CensusSource &census, ObligationSource obligations,
                     std::span<const transactions::Transaction> baseTxns,
                     bool baseTxnsSorted);

  [[nodiscard]] std::vector<transactions::Transaction>
  run(Execution execution, ::PhantomLedger::activity::spending::market::Market &market,
      const ::PhantomLedger::activity::spending::obligations::Snapshot &obligations,
      clearing::Ledger *screenBook = nullptr) const;

private:
  SpendingHabits habits_{};
  RunPlanning planning_{};
  DayPattern day_{};
  DynamicsProfile dynamics_{};
  EmissionProfile emission_{};
  CardLifecycleConfig cards_{};
};

} // namespace PhantomLedger::transfers::legit::routines::spending
