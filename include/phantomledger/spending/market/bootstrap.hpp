#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/spending/market/bounds.hpp"
#include "phantomledger/spending/market/cards.hpp"
#include "phantomledger/spending/market/commerce/network.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/market/population/census.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <array>
#include <cstdint>

namespace PhantomLedger::spending::market {

inline constexpr std::array<merchants::Category, 4> kBillerCategories{
    merchants::Category::utilities,
    merchants::Category::telecom,
    merchants::Category::insurance,
    merchants::Category::education,
};

[[nodiscard]] constexpr bool isBillerCategory(merchants::Category c) noexcept {
  for (const auto cat : kBillerCategories) {
    if (cat == c) {
      return true;
    }
  }
  return false;
}

struct MerchantPickBudget {
  std::uint16_t maxPickAttempts = 250;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] {
      v::ge("maxPickAttempts", static_cast<int>(maxPickAttempts), 1);
    });
  }
};

struct ExplorationDistribution {
  double alpha = 1.6;
  double beta = 9.5;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::gt("alpha", alpha, 0.0); });
    r.check([&] { v::gt("beta", beta, 0.0); });
  }
};

struct BurstSchedule {
  double probability = 0.08;
  std::uint16_t minDays = 3;
  std::uint16_t maxDays = 9;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("probability", probability); });
    r.check([&] { v::ge("minDays", static_cast<int>(minDays), 1); });
    r.check([&] {
      v::ge("maxDays", static_cast<int>(maxDays), static_cast<int>(minDays));
    });
  }
};

struct BootstrapInputs {
  Bounds bounds{};
  population::Census census{};
  commerce::Network network{};
  Cards cards{};

  MerchantPickBudget picking{};
  BurstSchedule burst{};
  ExplorationDistribution exploration{};
  std::uint64_t baseSeed = 0;

  // Counts plumbed from the world Merchants config.
  std::uint16_t favoriteMin = 8;
  std::uint16_t favoriteMax = 30;
  std::uint16_t billerMin = 2;
  std::uint16_t billerMax = 6;
};

[[nodiscard]] Market buildMarket(BootstrapInputs inputs);

} // namespace PhantomLedger::spending::market
