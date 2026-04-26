#pragma once

#include "phantomledger/spending/routing/channel.hpp"
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

struct RunPlan {
  std::vector<spenders::PreparedSpender> preparedSpenders;

  double targetTotalTxns = 0.0;
  std::uint64_t totalPersonDays = 0;

  std::span<const transactions::Transaction> baseTxns;
  std::span<const double> sensitivities;

  PaydayIndex paydayIndex;

  std::optional<std::uint32_t> personLimit;

  routing::ChannelCdf channelCdf{};
  routing::Policy routePolicy{};

  double baseExploreP = 0.0;
  double dayShockShape = 1.0;

  std::vector<clearing::Ledger::Index> personPrimaryIdx;

  std::vector<clearing::Ledger::Index> merchantCounterpartyIdx;

  clearing::Ledger::Index externalUnknownIdx = clearing::Ledger::invalid;
};

} // namespace PhantomLedger::spending::simulator
