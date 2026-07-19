#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>

namespace PhantomLedger::exporter::sinks {
struct PgMirror;
} // namespace PhantomLedger::exporter::sinks

namespace PhantomLedger::exporter::common {

struct TableCapture; // test seam; full definition in common/table.hpp

inline constexpr std::int64_t kFallbackEpoch = 1735689600;

struct ExportOptions {
  const synth::pii::PoolSet *piiPools = nullptr;

  // When set, every table is written directly into PostgreSQL as the
  // bytes the csv::Writer renders (see common/table.hpp). This is the
  // only production destination — PhantomLedger writes no files.
  const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

  // Test infrastructure: receives every table's rendered bytes, keyed
  // by stem (serverless exporter gates). Never set in production.
  TableCapture *capture = nullptr;
};

struct BaseSummary {
  std::size_t customerCount = 0;
  std::size_t internalAccountCount = 0;
  std::size_t counterpartyCount = 0;
  std::size_t totalTxnCount = 0;
  std::size_t illicitTxnCount = 0;
  std::size_t fraudRingCount = 0;
  std::size_t soloFraudCount = 0;
  std::size_t sarsFiledCount = 0;
};

[[nodiscard]] inline time::TimePoint
deriveSimStart(std::span<const transactions::Transaction> txns) noexcept {
  if (txns.empty()) {
    return time::fromEpochSeconds(kFallbackEpoch);
  }
  const auto min_tx =
      std::ranges::min(txns, {}, &transactions::Transaction::timestamp);
  return time::fromEpochSeconds(min_tx.timestamp);
}

[[nodiscard]] inline time::TimePoint
deriveSimEnd(std::span<const transactions::Transaction> txns) noexcept {
  if (txns.empty()) {
    return time::fromEpochSeconds(kFallbackEpoch);
  }
  const auto max_tx =
      std::ranges::max(txns, {}, &transactions::Transaction::timestamp);
  return time::fromEpochSeconds(max_tx.timestamp);
}

[[nodiscard]] inline const synth::pii::PoolSet &
requirePools(const ExportOptions &opts, std::string_view exporterName) {
  if (opts.piiPools == nullptr) {
    throw std::invalid_argument(
        std::format("exporter::{}::exportAll: Options::piiPools is null. "
                    "Set it to the PoolSet built by app::setup::buildPoolSet.",
                    exporterName));
  }
  return *opts.piiPools;
}

[[nodiscard]] inline std::size_t
countInternalAccounts(const entity::account::Registry &registry) noexcept {
  auto isInternal = [](const auto &rec) {
    return (rec.flags &
            entity::account::bit(entity::account::Flag::external)) == 0;
  };
  return static_cast<std::size_t>(
      std::ranges::count_if(registry.records, isInternal));
}

[[nodiscard]] inline std::size_t
countSoloFraud(const entity::person::Roster &roster) noexcept {
  auto isSoloFraud = [&](entity::PersonId p) {
    return roster.has(p, entity::person::Flag::soloFraud);
  };
  return static_cast<std::size_t>(std::ranges::count_if(
      std::views::iota(1u, roster.count + 1u), isSoloFraud));
}

[[nodiscard]] inline std::size_t
countIllicitTxns(std::span<const transactions::Transaction> txns) noexcept {
  return static_cast<std::size_t>(std::ranges::count_if(
      txns, [](const auto &tx) { return tx.fraud.flag != 0; }));
}

} // namespace PhantomLedger::exporter::common
