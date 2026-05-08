#pragma once

#include "phantomledger/primitives/time/calendar.hpp"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>

namespace PhantomLedger::cli {

[[nodiscard]] inline std::optional<std::int64_t>
parseInt(std::string_view s) noexcept {
  if (s.empty()) {
    return std::nullopt;
  }

  std::int64_t value{};
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] inline std::optional<std::uint64_t>
parseU64(std::string_view s) noexcept {
  if (s.empty()) {
    return std::nullopt;
  }

  int base = 10;
  if (s.starts_with("0x") || s.starts_with("0X")) {
    base = 16;
    s.remove_prefix(2);
  } else if (s.starts_with('0') && s.size() > 1) {
    base = 8;
    s.remove_prefix(1);
  }

  if (s.empty()) {
    return std::nullopt;
  }

  std::uint64_t value{};
  const auto [ptr, ec] =
      std::from_chars(s.data(), s.data() + s.size(), value, base);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] inline std::optional<::PhantomLedger::time::CalendarDate>
parseDate(std::string_view s) noexcept {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
    return std::nullopt;
  }

  const auto year = parseInt(s.substr(0, 4));
  const auto month = parseInt(s.substr(5, 2));
  const auto day = parseInt(s.substr(8, 2));

  if (!year || !month || !day || *year < 1 || *month < 1 || *month > 12 ||
      *day < 1 || *day > 31) {
    return std::nullopt;
  }

  return ::PhantomLedger::time::CalendarDate{
      .year = static_cast<int>(*year),
      .month = static_cast<unsigned>(*month),
      .day = static_cast<unsigned>(*day),
  };
}

} // namespace PhantomLedger::cli
