#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace PhantomLedger::exporter::common {

inline constexpr std::int64_t kFallbackEpoch = 1735689600;

struct ExportOptions {
  bool showTransactions = false;
  const synth::pii::PoolSet *piiPools = nullptr;
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

[[nodiscard]] inline csv::Writer openTable(const std::filesystem::path &dir,
                                           const schema::Table &table) {
  csv::Writer w{dir / std::filesystem::path(table.filename)};
  w.writeHeader(table.header);
  return w;
}

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

template <class SummaryLike>
inline void fillBaseCounts(SummaryLike &out, const pipeline::People &peopleData,
                           const pipeline::Holdings &holdingsData,
                           std::span<const transactions::Transaction> txns,
                           std::size_t counterpartyCount,
                           std::size_t sarsCount) noexcept {
  static_assert(std::is_base_of_v<BaseSummary, SummaryLike>,
                "fillBaseCounts: SummaryLike must inherit from BaseSummary");

  out.customerCount = peopleData.roster.roster.count;
  out.internalAccountCount =
      countInternalAccounts(holdingsData.accounts.registry);
  out.counterpartyCount = counterpartyCount;
  out.totalTxnCount = txns.size();
  out.illicitTxnCount = countIllicitTxns(txns);
  out.fraudRingCount = peopleData.roster.topology.rings.size();
  out.soloFraudCount = countSoloFraud(peopleData.roster.roster);
  out.sarsFiledCount = sarsCount;
}

} // namespace PhantomLedger::exporter::common
