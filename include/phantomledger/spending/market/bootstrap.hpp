#pragma once

#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/config/picking.hpp"
#include "phantomledger/spending/market/bounds.hpp"
#include "phantomledger/spending/market/commerce/network.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/market/population/census.hpp"

#include <cstdint>

namespace PhantomLedger::spending::market {

struct BootstrapInputs {
  Bounds bounds{};
  population::Census census{};
  commerce::Network network{};
  config::MerchantPickRules picking{};
  config::BurstBehavior burst{};
  config::ExplorationHabits exploration{};
  std::uint64_t baseSeed = 0;

  // Counts plumbed from the world Merchants config.
  std::uint16_t favoriteMin = 8;
  std::uint16_t favoriteMax = 30;
  std::uint16_t billerMin = 2;
  std::uint16_t billerMax = 6;
};

[[nodiscard]] Market buildMarket(const BootstrapInputs &inputs);

} // namespace PhantomLedger::spending::market
