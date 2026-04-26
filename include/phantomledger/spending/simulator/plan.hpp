#pragma once

#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/policy.hpp"
#include "phantomledger/spending/spenders/prepared.hpp"
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

  std::optional<std::uint32_t> personLimit;

  routing::ChannelCdf channelCdf{};

  routing::Policy routePolicy{};

  double baseExploreP = 0.0;

  double dayShockShape = 1.0;
};

} // namespace PhantomLedger::spending::simulator
