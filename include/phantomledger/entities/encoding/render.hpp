#pragma once

#include "phantomledger/entities/encoding/layout.hpp"
#include "phantomledger/entities/encoding/lookup.hpp"
#include "phantomledger/entities/encoding/numeric.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace PhantomLedger::encoding {

[[nodiscard]] constexpr std::size_t renderedSize(Layout layout,
                                                 std::uint64_t number) {
  if (!layout.allowZero && number == 0) {
    throw std::invalid_argument("ID sequence must be >= 1");
  }

  const auto numberDigits = digits(number);
  const auto paddedDigits = numberDigits < layout.width
                                ? static_cast<std::size_t>(layout.width)
                                : numberDigits;

  return layout.prefix.size() + paddedDigits;
}

[[nodiscard]] inline std::size_t renderedSize(const entity::Key &id) {
  const auto spec = checkedLayout(id);
  return renderedSize(spec, id.number);
}

inline std::size_t write(char *out, Layout layout, std::uint64_t number) {
  if (!layout.allowZero && number == 0) {
    throw std::invalid_argument("ID sequence must be >= 1");
  }

  std::memcpy(out, layout.prefix.data(), layout.prefix.size());

  const auto numberDigits = digits(number);
  const auto paddedDigits = numberDigits < layout.width
                                ? static_cast<std::size_t>(layout.width)
                                : numberDigits;

  char *begin = out + layout.prefix.size();
  char *pos = begin + paddedDigits;

  do {
    *--pos = static_cast<char>('0' + (number % 10));
    number /= 10;
  } while (number != 0);

  while (pos != begin) {
    *--pos = '0';
  }

  return layout.prefix.size() + paddedDigits;
}

inline std::size_t write(char *out, const entity::Key &id) {
  const auto spec = checkedLayout(id);
  return write(out, spec, id.number);
}

// ────────────────────────────────────────────────────────────────────
// RenderedId<N> — fixed-capacity inline string buffer.
//
// Returned by `format(Key)` and the various encoders in
// entities/encoding/*.hpp. Callers can:
//   • pass it directly to csv::Writer / any string_view consumer (the
//     implicit conversion below makes it act like a string_view in
//     argument position),
//   • call .view() to get a string_view explicitly,
//   • copy its bytes into a std::string via `std::string{rk.view()}`
//     at the boundary where ownership is required.
//
// LIFETIME: the implicit string_view conversion is non-owning. It
// borrows from the RenderedId's `bytes` array. Same rule as taking
// a string_view of any local std::string — don't outlive the source.
// In practice every call site uses the temporary inside a single
// statement (writeRow, comparison, copy into a container), where the
// temporary's lifetime extends to the end of the full expression.
// ────────────────────────────────────────────────────────────────────

template <std::size_t N> struct RenderedId {
  std::array<char, N> bytes{};
  std::uint8_t length = 0;

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    return std::string_view{bytes.data(), length};
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return length == 0; }

  // Implicit conversion: lets `RenderedId` be passed wherever a
  // `string_view` is expected (csv::Writer::cell, writeRow, ...).
  // Zero-cost — just exposes the underlying buffer.
  constexpr operator std::string_view() const noexcept { return view(); }

  [[nodiscard]] friend constexpr bool
  operator==(const RenderedId &lhs, std::string_view rhs) noexcept {
    return lhs.view() == rhs;
  }
};

using RenderedKey = RenderedId<32>;

[[nodiscard]] inline RenderedKey format(const entity::Key &id) {
  RenderedKey out;
  out.length = static_cast<std::uint8_t>(write(out.bytes.data(), id));
  return out;
}

} // namespace PhantomLedger::encoding
