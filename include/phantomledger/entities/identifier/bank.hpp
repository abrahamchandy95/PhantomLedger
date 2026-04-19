#pragma once

#include <cstdint>

namespace PhantomLedger::entities::identifier {

enum class Bank : std::uint8_t {
  internal,
  external,
};

} // namespace PhantomLedger::entities::identifier
