#pragma once

#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/emission_result.hpp"
#include "phantomledger/spending/routing/policy.hpp"
#include "phantomledger/spending/simulator/payday_index.hpp"
#include "phantomledger/spending/spenders/prepared.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

struct PopulationPlan {
  std::vector<spenders::PreparedSpender> spenders;
  std::span<const double> sensitivities;

  [[nodiscard]] std::uint32_t activeCount() const noexcept {
    return static_cast<std::uint32_t>(spenders.size());
  }
};

struct TransactionBudget {
  double targetTotalTxns = 0.0;
  std::uint64_t totalPersonDays = 0;
  std::optional<std::uint32_t> personLimit;
};

struct BaseLedgerPlan {
  std::span<const transactions::Transaction> txns;
};

struct PaydayPlan {
  PaydayIndex index;
};

struct RoutingPlan {
  routing::ChannelCdf channelCdf{};
  routing::Policy policy{};

  std::vector<clearing::Ledger::Index> personPrimaryIdx;
  std::vector<clearing::Ledger::Index> merchantCounterpartyIdx;
  clearing::Ledger::Index externalUnknownIdx = clearing::Ledger::invalid;

  [[nodiscard]] routing::ResolvedAccounts resolvedAccounts() const noexcept {
    return routing::ResolvedAccounts{
        .personPrimaryIdx =
            std::span<const clearing::Ledger::Index>(personPrimaryIdx),
        .merchantCounterpartyIdx =
            std::span<const clearing::Ledger::Index>(merchantCounterpartyIdx),
        .externalUnknownIdx = externalUnknownIdx,
    };
  }
};

struct ActivityPlan {
  double baseExploreP = 0.0;
  double dayShockShape = 1.0;
};

struct RunPlan {
  PopulationPlan population;
  TransactionBudget budget;
  BaseLedgerPlan baseLedger;
  PaydayPlan payday;
  RoutingPlan routing;
  ActivityPlan activity;
};

} // namespace PhantomLedger::spending::simulator
