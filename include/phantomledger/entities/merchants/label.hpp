#pragma once

#include <cstdint>

namespace PhantomLedger::entities::merchants {

struct Label {
  std::uint64_t value = 0;

  friend constexpr bool operator==(const Label &lhs,
                                   const Label &rhs) noexcept = default;
};

} // namespace PhantomLedger::entities::merchants
