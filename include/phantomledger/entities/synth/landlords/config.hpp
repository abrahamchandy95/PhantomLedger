#pragma once

#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <array>

namespace PhantomLedger::entities::synth::landlords {

namespace enumTax = ::PhantomLedger::taxonomies::enums;
namespace landlord = ::PhantomLedger::entity::landlord;

struct Share {
  landlord::Type type = landlord::Type::individual;
  double weight = 0.0;
};

struct Rate {
  landlord::Type type = landlord::Type::individual;
  double value = 0.0;
};

namespace detail {

[[nodiscard]] constexpr std::array<double, landlord::kTypeCount>
rates(std::array<Rate, landlord::kTypeCount> entries) noexcept {
  std::array<double, landlord::kTypeCount> out{};

  for (const auto &entry : entries) {
    out[enumTax::toIndex(entry.type)] = entry.value;
  }

  return out;
}

} // namespace detail

struct InBankProbability {
  std::array<double, landlord::kTypeCount> byType = detail::rates({{
      {landlord::Type::individual, 0.06},
      {landlord::Type::llcSmall, 0.04},
      {landlord::Type::corporate, 0.01},
  }});

  [[nodiscard]] constexpr double forType(landlord::Type type) const noexcept {
    return byType[enumTax::toIndex(type)];
  }
};

struct Config {
  double perTenK = 12.0;
  int floor = 3;

  std::array<Share, landlord::kTypeCount> mix{{
      {landlord::Type::individual, 0.38},
      {landlord::Type::llcSmall, 0.15},
      {landlord::Type::corporate, 0.47},
  }};

  InBankProbability inBankP;
};

} // namespace PhantomLedger::entities::synth::landlords
