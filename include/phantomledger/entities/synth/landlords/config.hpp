#pragma once

#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <array>

namespace PhantomLedger::entities::synth::landlords {

using namespace ::PhantomLedger::taxonomies::enums;
using namespace ::PhantomLedger::entity::landlord;

struct Share {
  Type type = Type::individual;
  double weight = 0.0;
};

struct Rate {
  Type type = Type::individual;
  double value = 0.0;
};

namespace detail {

[[nodiscard]] constexpr std::array<double, kTypeCount>
rates(std::array<Rate, kTypeCount> entries) noexcept {
  std::array<double, kTypeCount> out{};

  for (const auto &entry : entries) {
    out[toIndex(entry.type)] = entry.value;
  }

  return out;
}

} // namespace detail

struct InBankProbability {
  std::array<double, kTypeCount> byType = detail::rates({{
      {Type::individual, 0.06},
      {Type::llcSmall, 0.04},
      {Type::corporate, 0.01},
  }});

  [[nodiscard]] constexpr double forType(Type type) const noexcept {
    return byType[toIndex(type)];
  }
};

struct Config {
  double perTenK = 12.0;
  int floor = 3;

  std::array<Share, kTypeCount> mix{{
      {Type::individual, 0.38},
      {Type::llcSmall, 0.15},
      {Type::corporate, 0.47},
  }};

  InBankProbability inBankP;
};

} // namespace PhantomLedger::entities::synth::landlords
