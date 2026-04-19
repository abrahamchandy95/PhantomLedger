#pragma once

#include <cstdint>

namespace PhantomLedger::entities::landlords {

enum class Class : std::uint8_t {
  unspecified = 0,
  individual = 1,
  llcSmall = 2,
  corporate = 3,
};

} // namespace PhantomLedger::entities::landlords
