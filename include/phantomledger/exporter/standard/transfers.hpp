#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/standard/infra.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/network/format.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::standard {

namespace detail {

struct PairKey {
  ::PhantomLedger::entity::Key source;
  ::PhantomLedger::entity::Key target;

  constexpr bool operator==(const PairKey &) const noexcept = default;
};

struct PairKeyHash {
  std::size_t operator()(const PairKey &k) const noexcept {
    const auto h1 = std::hash<::PhantomLedger::entity::Key>{}(k.source);
    const auto h2 = std::hash<::PhantomLedger::entity::Key>{}(k.target);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
  }
};

struct Aggregate {
  double totalAmount = 0.0;
  std::uint64_t txnCount = 0;
  std::int64_t firstTs = 0;
  std::int64_t lastTs = 0;
};

[[nodiscard]] inline std::unordered_map<PairKey, Aggregate, PairKeyHash>
aggregateHasPaid(
    std::span<const ::PhantomLedger::transactions::Transaction> txns) {
  std::unordered_map<PairKey, Aggregate, PairKeyHash> agg;
  agg.reserve(txns.size() / 2 + 1);

  for (const auto &tx : txns) {
    const PairKey key{tx.source, tx.target};
    auto it = agg.find(key);
    if (it == agg.end()) {
      Aggregate fresh{};
      fresh.totalAmount = tx.amount;
      fresh.txnCount = 1;
      fresh.firstTs = tx.timestamp;
      fresh.lastTs = tx.timestamp;
      agg.emplace(key, fresh);
    } else {
      auto &rec = it->second;
      rec.totalAmount += tx.amount;
      ++rec.txnCount;
      if (tx.timestamp < rec.firstTs) {
        rec.firstTs = tx.timestamp;
      } else if (tx.timestamp > rec.lastTs) {
        rec.lastTs = tx.timestamp;
      }
    }
  }
  return agg;
}

/// Round to 2 decimal places, matching `round(total_amount, 2)`.
[[nodiscard]] inline double round2(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

} // namespace detail

inline void writeHasPaidRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns) {
  const auto agg = detail::aggregateHasPaid(finalTxns);

  std::vector<std::pair<detail::PairKey, const detail::Aggregate *>> entries;
  entries.reserve(agg.size());
  for (const auto &kv : agg) {
    entries.emplace_back(kv.first, &kv.second);
  }
  std::sort(entries.begin(), entries.end(),
            [](const auto &a, const auto &b) noexcept {
              if (a.first.source != b.first.source) {
                return a.first.source < b.first.source;
              }
              return a.first.target < b.first.target;
            });

  for (const auto &[key, recPtr] : entries) {
    const auto &rec = *recPtr;
    w.writeRow(::PhantomLedger::encoding::format(key.source),
               ::PhantomLedger::encoding::format(key.target),
               detail::round2(rec.totalAmount), rec.txnCount,
               ::PhantomLedger::time::formatTimestamp(
                   ::PhantomLedger::time::fromEpochSeconds(rec.firstTs)),
               ::PhantomLedger::time::formatTimestamp(
                   ::PhantomLedger::time::fromEpochSeconds(rec.lastTs)));
  }
}

inline void writeLedgerRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns) {
  for (const auto &tx : finalTxns) {

    const std::uint32_t ringId =
        tx.fraud.ringId.has_value() ? *tx.fraud.ringId : 0U;

    w.writeRow(::PhantomLedger::encoding::format(tx.source),
               ::PhantomLedger::encoding::format(tx.target), tx.amount,
               ::PhantomLedger::time::formatTimestamp(
                   ::PhantomLedger::time::fromEpochSeconds(tx.timestamp)),
               static_cast<std::uint32_t>(tx.fraud.flag), ringId,
               detail::renderDeviceId(tx.session.deviceId),
               ::PhantomLedger::network::format(tx.session.ipAddress),
               ::PhantomLedger::channels::name(tx.session.channel));
  }
}

} // namespace PhantomLedger::exporter::standard
