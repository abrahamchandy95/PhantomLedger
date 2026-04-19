#pragma once

#include "phantomledger/entities/landlords/class.hpp"

#include <vector>

namespace PhantomLedger::entities::synth::landlords {

struct Share {
  entities::landlords::Class kind = entities::landlords::Class::individual;
  double weight = 0.0;
};

struct Config {
  double perTenK = 12.0;
  int floor = 3;

  std::vector<Share> mix = {
      {entities::landlords::Class::individual, 0.38},
      {entities::landlords::Class::llcSmall, 0.15},
      {entities::landlords::Class::corporate, 0.47},
  };
};

} // namespace PhantomLedger::entities::synth::landlords
