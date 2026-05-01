#pragma once

#include "phantomledger/entities/landlords.hpp"

#include <array>

namespace PhantomLedger::entities::synth::landlords {

struct Share {
  entity::landlord::Class kind = entity::landlord::Class::individual;
  double weight = 0.0;
};

/// Per-type probability that a landlord also banks at our institution.
struct InBankProbability {
  std::array<double, 3> byClass{
      0.06, // individual
      0.04, // llcSmall
      0.01, // corporate
  };

  [[nodiscard]] constexpr double
  forClass(entity::landlord::Class kind) const noexcept {
    return byClass[entity::landlord::classIndex(kind)];
  }
};
struct Config {
  double perTenK = 12.0;
  int floor = 3;

  std::array<Share, 3> mix{{
      {entity::landlord::Class::individual, 0.38},
      {entity::landlord::Class::llcSmall, 0.15},
      {entity::landlord::Class::corporate, 0.47},
  }};

  InBankProbability inBankP;
};

} // namespace PhantomLedger::entities::synth::landlords
