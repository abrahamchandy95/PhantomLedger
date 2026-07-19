#pragma once

#include "phantomledger/primitives/hashing/fnv.hpp"

#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace PhantomLedger::exporter::common {

[[nodiscard]] inline std::uint64_t
stableU64(std::initializer_list<std::string_view> parts) noexcept {
  return ::PhantomLedger::hashing::fnv1a64Join(parts, '|');
}

} // namespace PhantomLedger::exporter::common
