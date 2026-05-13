#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace PhantomLedger::exporter::common {

inline constexpr std::int64_t kFallbackEpoch = 1735689600;

struct ExportOptions {
  bool showTransactions = false;
  const ::PhantomLedger::entities::synth::pii::PoolSet *piiPools = nullptr;
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

[[nodiscard]] inline ::PhantomLedger::exporter::csv::Writer
openTable(const std::filesystem::path &dir,
          const ::PhantomLedger::exporter::schema::Table &table) {
  ::PhantomLedger::exporter::csv::Writer w{
      dir / std::filesystem::path(table.filename)};
  w.writeHeader(table.header);
  return w;
}

[[nodiscard]] inline ::PhantomLedger::time::TimePoint deriveSimStart(
    std::span<const ::PhantomLedger::transactions::Transaction> txns) noexcept {
  if (txns.empty()) {
    return ::PhantomLedger::time::fromEpochSeconds(kFallbackEpoch);
  }
  std::int64_t minTs = txns.front().timestamp;
  for (const auto &tx : txns) {
    if (tx.timestamp < minTs) {
      minTs = tx.timestamp;
    }
  }
  return ::PhantomLedger::time::fromEpochSeconds(minTs);
}

[[nodiscard]] inline ::PhantomLedger::time::TimePoint deriveSimEnd(
    std::span<const ::PhantomLedger::transactions::Transaction> txns) noexcept {
  if (txns.empty()) {
    return ::PhantomLedger::time::fromEpochSeconds(kFallbackEpoch);
  }
  std::int64_t maxTs = txns.front().timestamp;
  for (const auto &tx : txns) {
    if (tx.timestamp > maxTs) {
      maxTs = tx.timestamp;
    }
  }
  return ::PhantomLedger::time::fromEpochSeconds(maxTs);
}

[[nodiscard]] inline const ::PhantomLedger::entities::synth::pii::PoolSet &
requirePools(const ExportOptions &opts, std::string_view exporterName) {
  if (opts.piiPools == nullptr) {
    std::string msg;
    msg.reserve(64 + exporterName.size());
    msg.append("exporter::")
        .append(exporterName)
        .append("::exportAll: Options::piiPools is null. ")
        .append("Set it to the PoolSet built by app::setup::buildPoolSet.");
    throw std::invalid_argument(msg);
  }
  return *opts.piiPools;
}

[[nodiscard]] inline std::size_t countInternalAccounts(
    const ::PhantomLedger::entity::account::Registry &registry) noexcept {
  std::size_t count = 0;
  for (const auto &rec : registry.records) {
    if ((rec.flags & ::PhantomLedger::entity::account::bit(
                         ::PhantomLedger::entity::account::Flag::external)) ==
        0) {
      ++count;
    }
  }
  return count;
}

[[nodiscard]] inline std::size_t
countSoloFraud(const ::PhantomLedger::entity::person::Roster &roster) noexcept {
  std::size_t count = 0;
  for (::PhantomLedger::entity::PersonId p = 1; p <= roster.count; ++p) {
    if (roster.has(p, ::PhantomLedger::entity::person::Flag::soloFraud)) {
      ++count;
    }
  }
  return count;
}

[[nodiscard]] inline std::size_t countIllicitTxns(
    std::span<const ::PhantomLedger::transactions::Transaction> txns) noexcept {
  return static_cast<std::size_t>(std::count_if(
      txns.begin(), txns.end(),
      [](const ::PhantomLedger::transactions::Transaction &tx) noexcept {
        return tx.fraud.flag != 0;
      }));
}

template <class SummaryLike>
inline void
fillBaseCounts(SummaryLike &out,
               const ::PhantomLedger::pipeline::Entities &entities,
               std::span<const ::PhantomLedger::transactions::Transaction> txns,
               std::size_t counterpartyCount, std::size_t sarsCount) noexcept {
  static_assert(std::is_base_of_v<BaseSummary, SummaryLike>,
                "fillBaseCounts: SummaryLike must inherit from BaseSummary");
  out.customerCount = entities.people.roster.count;
  out.internalAccountCount = countInternalAccounts(entities.accounts.registry);
  out.counterpartyCount = counterpartyCount;
  out.totalTxnCount = txns.size();
  out.illicitTxnCount = countIllicitTxns(txns);
  out.fraudRingCount = entities.people.topology.rings.size();
  out.soloFraudCount = countSoloFraud(entities.people.roster);
  out.sarsFiledCount = sarsCount;
}

} // namespace PhantomLedger::exporter::common
