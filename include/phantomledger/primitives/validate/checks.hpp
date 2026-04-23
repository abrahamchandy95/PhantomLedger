#pragma once
/*
 * checks.hpp — compile-time and runtime numeric validation.
 */

#include <cmath>
#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::validate {

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

namespace detail {

// Manual to_string that works for both int and double at compile time.
inline std::string numStr(Numeric auto v) {
  if constexpr (std::integral<decltype(v)>) {
    return std::to_string(v);
  } else {
    // Avoid trailing zeros: 3.500000 -> "3.5"
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    return std::string(buf);
  }
}

[[noreturn]] inline void fail(std::string_view name, std::string_view op,
                              std::string_view bound, std::string_view actual) {
  std::string msg;
  msg.reserve(name.size() + op.size() + bound.size() + actual.size() + 32);
  msg += name;
  msg += " must be ";
  msg += op;
  msg += ' ';
  msg += bound;
  msg += ", got ";
  msg += actual;
  throw std::invalid_argument(msg);
}

} // namespace detail

inline void gt(std::string_view name, Numeric auto value, Numeric auto lo) {
  if (!(value > lo)) {
    detail::fail(name, ">", detail::numStr(lo), detail::numStr(value));
  }
}

inline void ge(std::string_view name, Numeric auto value, Numeric auto lo) {
  if (!(value >= lo)) {
    detail::fail(name, ">=", detail::numStr(lo), detail::numStr(value));
  }
}

inline void between(std::string_view name, Numeric auto value, Numeric auto lo,
                    Numeric auto hi) {
  if (!(value >= lo && value <= hi)) {
    std::string range =
        "[" + detail::numStr(lo) + ", " + detail::numStr(hi) + "]";
    detail::fail(name, "in", range, detail::numStr(value));
  }
}

inline void finite(std::string_view name, double value) {
  if (!std::isfinite(value)) {
    detail::fail(name, "finite, got", detail::numStr(value), "");
  }
}

inline void positive(std::string_view name, Numeric auto value) {
  gt(name, value, decltype(value){0});
}

inline void nonNegative(std::string_view name, Numeric auto value) {
  ge(name, value, decltype(value){0});
}

} // namespace PhantomLedger::validate
