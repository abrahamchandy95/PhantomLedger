#pragma once

#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace PhantomLedger::exporter::common {

[[nodiscard]] inline std::uint64_t
stableU64(std::initializer_list<std::string_view> parts) noexcept {
  constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
  constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;
  std::uint64_t h = kFnvOffsetBasis;
  for (const auto part : parts) {
    for (const auto c : part) {
      h ^= static_cast<std::uint8_t>(c);
      h *= kFnvPrime;
    }
    h ^= static_cast<std::uint8_t>('|');
    h *= kFnvPrime;
  }
  return h;
}

} // namespace PhantomLedger::exporter::common
