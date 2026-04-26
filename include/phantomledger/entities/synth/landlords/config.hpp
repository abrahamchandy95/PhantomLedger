#pragma once

#include "phantomledger/entities/landlords.hpp"

#include <array>

namespace PhantomLedger::entities::synth::landlords {

struct Share {
  entity::landlord::Class kind = entity::landlord::Class::individual;
  double weight = 0.0;
};

/// Per-type probability that a landlord also banks at our institution.
/// Matches Python common/config/population/landlords.py _default_in_bank_p().
///
/// Research basis (NFIB 2023, FDIC SOD 2024):
///   individual: ~6%  (local personal banking customer)
///   llcSmall:   ~4%  (intentional separate business banking)
///   corporate:  ~1%  (commercial banking, national scope)
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
