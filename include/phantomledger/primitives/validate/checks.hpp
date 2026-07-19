#pragma once

#include <cmath>
#include <concepts>
#include <cstdio>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PhantomLedger::primitives::validate {

template <class T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// -----------------------------------------------------------------------------
// Error type
// -----------------------------------------------------------------------------

class Error : public std::invalid_argument {
public:
  Error(std::string message, std::source_location where)
      : std::invalid_argument(std::move(message)), where_(where) {}

  [[nodiscard]] std::source_location where() const noexcept { return where_; }

private:
  std::source_location where_;
};

// -----------------------------------------------------------------------------
// Formatting (lazy — only touched on the failure path)
// -----------------------------------------------------------------------------

namespace detail {

template <Numeric T> [[nodiscard]] inline std::string fmt(T v) {
  if constexpr (std::integral<T>) {
    return std::to_string(v);
  } else {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", static_cast<double>(v));
    return std::string{buf};
  }
}

[[noreturn]] inline void raise(std::string_view field,
                               std::string_view predicate,
                               std::string_view bound, std::string_view actual,
                               std::source_location loc) {
  std::string msg;
  msg.reserve(field.size() + predicate.size() + bound.size() + actual.size() +
              24);
  msg.append(field);
  msg.append(" must ");
  msg.append(predicate);
  if (!bound.empty()) {
    msg.append(" ");
    msg.append(bound);
  }
  msg.append("; got ");
  msg.append(actual);
  throw Error(std::move(msg), loc);
}

} // namespace detail

// -----------------------------------------------------------------------------
// Individual checks (eager throw). Defaulted source_location captures
// the caller, not this file.
// -----------------------------------------------------------------------------

template <Numeric T, Numeric U>
constexpr void gt(std::string_view field, T value, U lo,
                  std::source_location loc = std::source_location::current()) {
  if (!(value > static_cast<T>(lo))) {
    detail::raise(field, "be >", detail::fmt(lo), detail::fmt(value), loc);
  }
}

template <Numeric T, Numeric U>
constexpr void ge(std::string_view field, T value, U lo,
                  std::source_location loc = std::source_location::current()) {
  if (!(value >= static_cast<T>(lo))) {
    detail::raise(field, "be >=", detail::fmt(lo), detail::fmt(value), loc);
  }
}

template <Numeric T, Numeric U>
constexpr void lt(std::string_view field, T value, U hi,
                  std::source_location loc = std::source_location::current()) {
  if (!(value < static_cast<T>(hi))) {
    detail::raise(field, "be <", detail::fmt(hi), detail::fmt(value), loc);
  }
}

template <Numeric T, Numeric U, Numeric V>
constexpr void
between(std::string_view field, T value, U lo, V hi,
        std::source_location loc = std::source_location::current()) {
  if (!(value >= static_cast<T>(lo) && value <= static_cast<T>(hi))) {
    std::string range;
    range.reserve(detail::fmt(lo).size() + detail::fmt(hi).size() + 4);
    range.append("[")
        .append(detail::fmt(lo))
        .append(", ")
        .append(detail::fmt(hi))
        .append("]");
    detail::raise(field, "lie in", range, detail::fmt(value), loc);
  }
}

inline void finite(std::string_view field, double value,
                   std::source_location loc = std::source_location::current()) {
  if (!std::isfinite(value)) {
    detail::raise(field, "be finite", {}, detail::fmt(value), loc);
  }
}

template <Numeric T>
constexpr void
positive(std::string_view field, T value,
         std::source_location loc = std::source_location::current()) {
  gt(field, value, T{0}, loc);
}

template <Numeric T>
constexpr void
nonNegative(std::string_view field, T value,
            std::source_location loc = std::source_location::current()) {
  ge(field, value, T{0}, loc);
}

inline void unit(std::string_view field, double value,
                 std::source_location loc = std::source_location::current()) {
  between(field, value, 0.0, 1.0, loc);
}

// -----------------------------------------------------------------------------
// Report — batch accumulator. Useful when validating a whole Policy and you
// want to see every failure at once, not just the first.
// -----------------------------------------------------------------------------

class Report {
public:
  template <class Fn> void check(Fn &&fn) noexcept {
    try {
      std::forward<Fn>(fn)();
    } catch (const Error &e) {
      messages_.emplace_back(e.what());
    }
  }

  [[nodiscard]] bool ok() const noexcept { return messages_.empty(); }
  [[nodiscard]] const auto &messages() const noexcept { return messages_; }

  void throwIfFailed(
      std::source_location loc = std::source_location::current()) const {
    if (ok())
      return;
    std::string agg;
    for (const auto &m : messages_) {
      agg.append(m).append("\n");
    }
    if (!agg.empty())
      agg.pop_back();
    throw Error(std::move(agg), loc);
  }

private:
  std::vector<std::string> messages_;
};

// -----------------------------------------------------------------------------
// Validator concept — so structs can opt into validation uniformly.
// -----------------------------------------------------------------------------

template <class T>
concept Validatable = requires(const T &t, Report &r) {
  { t.validate(r) } -> std::same_as<void>;
};

template <Validatable T> inline void require(const T &t) {
  Report r;
  t.validate(r);
  r.throwIfFailed();
}

} // namespace PhantomLedger::primitives::validate
