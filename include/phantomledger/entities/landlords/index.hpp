#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::landlords {

struct Index {
  std::array<std::vector<std::uint32_t>, 3> byClass;
};

} // namespace PhantomLedger::entities::landlords
