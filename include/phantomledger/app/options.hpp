#pragma once

#include "phantomledger/primitives/time/calendar.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace PhantomLedger::app {

enum class UseCase : std::uint8_t {
  standard = 0,
  muleMl = 1,
  aml = 2,
  amlTxnEdges = 3,
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
  return std::nullopt;
}

inline constexpr std::array<UseCase, 4> kAllUseCases{
    UseCase::standard,
    UseCase::muleMl,
    UseCase::aml,
    UseCase::amlTxnEdges,
};

// ------------------------------------------------------------------ engine
//
// The corpus engine is an implementation detail, not a use case: both
// engines are proven byte-identical for every export, so selection is
// automatic and there is NO user-facing flag. The memory-unbounded
// monolithic REFERENCE engine — the path the equivalence gates compare
// against — stays reachable only through the PL_ENGINE environment
// variable (test infrastructure, not product).

enum class Engine : std::uint8_t {
  automatic = 0,  // resolveEngine: windowed for every use case
  windowed = 1,   // bounded-memory two-phase fold (the production engine)
  monolithic = 2, // memory-unbounded reference engine
};

[[nodiscard]] constexpr std::string_view name(Engine engine) noexcept {
  switch (engine) {
  case Engine::automatic:
    return "auto";
  case Engine::windowed:
    return "windowed";
  case Engine::monolithic:
    return "monolithic";
  }
  return "<unknown>";
}

// Parses the PL_ENGINE environment variable (there is no CLI flag);
// the absence of the variable means Engine::automatic.
[[nodiscard]] constexpr std::optional<Engine>
parseEngine(std::string_view s) noexcept {
  if (s == "auto") {
    return Engine::automatic;
  }
  if (s == "windowed") {
    return Engine::windowed;
  }
  if (s == "monolithic") {
    return Engine::monolithic;
  }
  return std::nullopt;
}

// Every use case runs windowed: PostgreSQL is a stated requirement of
// every run (backend policy — the binary fails fast before generation
// when no server answers), which is exactly what the aml-txn-edges
// windowed path needs, since its derived analytics read the streamed
// corpus back from the transactions table. Under the PL_FILE_ONLY=1
// harness escape, aml-txn-edges fails with its own clear error rather
// than silently switching engines: resolution stays a pure function of
// the use case — never of server reachability or data size.
[[nodiscard]] constexpr bool supportsWindowed(UseCase /*uc*/) noexcept {
  return true;
}

[[nodiscard]] constexpr Engine resolveEngine(Engine requested,
                                             UseCase uc) noexcept {
  if (requested != Engine::automatic) {
    return requested;
  }
  return supportsWindowed(uc) ? Engine::windowed : Engine::monolithic;
}

struct RunOptions {
  UseCase usecase = UseCase::standard;
  std::int64_t days = 365;
  std::int32_t population = 70'000;
  std::uint64_t seed = 0xDEADBEEFULL;
  std::filesystem::path outDir = "out_bank_data";
  bool showTransactions = false;

  // Corpus engine; automatic resolves per use case (resolveEngine).
  // Populated only from the PL_ENGINE environment variable — test
  // infrastructure, never a CLI flag. Output bytes are identical
  // either way; only memory behavior and the PostgreSQL span
  // bookkeeping differ.
  Engine engine = Engine::automatic;

  std::string pgConninfo = "dbname=phantomledger";
  ::PhantomLedger::time::CalendarDate startDate{2025, 1, 1};
};

} // namespace PhantomLedger::app
