#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace PhantomLedger::encoding {

[[nodiscard]] constexpr std::size_t digits(std::uint64_t value) noexcept {
  std::size_t count = 1;
  while (value >= 10) {
    value /= 10;
    ++count;
  }
  return count;
}

inline void appendUnsigned(std::string &out, std::uint64_t value) {
  const auto count = digits(value);
  const auto start = out.size();

  out.resize(start + count);
  auto *pos = out.data() + out.size();

  do {
    *--pos = static_cast<char>('0' + (value % 10));
    value /= 10;
  } while (value != 0);
}

} // namespace PhantomLedger::encoding
