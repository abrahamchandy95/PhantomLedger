#pragma once

#include "phantomledger/primitives/time/calendar.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace PhantomLedger::app {

enum class UseCase : std::uint8_t {
  standard = 0,
  muleMl = 1,
  aml = 2,
  amlTxnEdges = 3,
  cardFraud = 4,
};

[[nodiscard]] constexpr std::string_view name(UseCase uc) noexcept {
  switch (uc) {
  case UseCase::standard:
    return "standard";
  case UseCase::muleMl:
    return "mule-ml";
  case UseCase::aml:
    return "aml";
  case UseCase::amlTxnEdges:
    return "aml-txn-edges";
  case UseCase::cardFraud:
    return "card-fraud";
  }
  return "<unknown>";
}

[[nodiscard]] constexpr std::optional<UseCase>
parseUseCase(std::string_view s) noexcept {
  if (s == "standard") {
    return UseCase::standard;
  }
  if (s == "mule-ml") {
    return UseCase::muleMl;
  }
  if (s == "aml") {
    return UseCase::aml;
  }
  if (s == "aml-txn-edges") {
    return UseCase::amlTxnEdges;
  }
  if (s == "card-fraud") {
    return UseCase::cardFraud;
  }
  return std::nullopt;
}

inline constexpr std::array<UseCase, 5> kAllUseCases{
    UseCase::standard,     UseCase::muleMl,    UseCase::aml,
    UseCase::amlTxnEdges,  UseCase::cardFraud,
};

struct RunOptions {
  UseCase usecase = UseCase::standard;
  std::int64_t days = 365;
  std::int32_t population = 70'000;
  std::uint64_t seed = 0xDEADBEEFULL;

  std::string pgConninfo = "dbname=phantomledger";
  ::PhantomLedger::time::CalendarDate startDate{2025, 1, 1};
};

} // namespace PhantomLedger::app
