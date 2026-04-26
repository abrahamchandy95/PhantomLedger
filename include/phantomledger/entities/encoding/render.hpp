#pragma once

#include "phantomledger/entities/encoding/layout.hpp"
#include "phantomledger/entities/encoding/lookup.hpp"
#include "phantomledger/entities/encoding/numeric.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

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

[[nodiscard]] inline std::string render(Layout layout, std::uint64_t number) {
  std::string out;
  out.resize(renderedSize(layout, number));
  write(out.data(), layout, number);
  return out;
}

[[nodiscard]] inline std::string format(const entity::Key &id) {
  const auto spec = checkedLayout(id);
  return render(spec, id.number);
}

} // namespace PhantomLedger::encoding
