#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::standard {

namespace detail {

namespace enc = ::PhantomLedger::encoding;
namespace t = ::PhantomLedger::time;
namespace tx_ns = ::PhantomLedger::transactions;
namespace ent = ::PhantomLedger::entity;

struct Aggregate {
  double totalAmount = 0.0;
  std::uint64_t txnCount = 0;
  std::int64_t firstTs = 0;
  std::int64_t lastTs = 0;
};

using AggregateMap =
    std::unordered_map<ent::KeyPair, Aggregate, ent::KeyPairHash>;

[[nodiscard]] inline AggregateMap
aggregateHasPaid(std::span<const tx_ns::Transaction> txns) {
  AggregateMap agg;
  agg.reserve(txns.size() / 2 + 1);

  for (const auto &tx : txns) {
    const ent::KeyPair key{tx.source, tx.target};
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

inline void writePairCells(::PhantomLedger::exporter::csv::Writer &w,
                           const ent::KeyPair &key) {
  w.cell(enc::format(key.source).view()).cell(enc::format(key.target).view());
}

inline void writeAggregateCells(::PhantomLedger::exporter::csv::Writer &w,
                                const Aggregate &rec) {
  w.cell(::PhantomLedger::primitives::utils::roundMoney(rec.totalAmount))
      .cell(rec.txnCount);
}

inline void writeTimeRangeCells(::PhantomLedger::exporter::csv::Writer &w,
                                const Aggregate &rec) {
  w.cell(t::formatTimestamp(t::fromEpochSeconds(rec.firstTs)))
      .cell(t::formatTimestamp(t::fromEpochSeconds(rec.lastTs)));
}

inline void writeHasPaidRow(::PhantomLedger::exporter::csv::Writer &w,
                            const ent::KeyPair &key, const Aggregate &rec) {
  writePairCells(w, key);
  writeAggregateCells(w, rec);
  writeTimeRangeCells(w, rec);
  w.endRow();
}

// Sort aggregate entries by (source, target).
[[nodiscard]] inline std::vector<std::pair<ent::KeyPair, const Aggregate *>>
sortedEntries(const AggregateMap &agg) {
  std::vector<std::pair<ent::KeyPair, const Aggregate *>> entries;
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
  return entries;
}

} // namespace detail

inline void writeHasPaidRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns) {
  const auto agg = detail::aggregateHasPaid(finalTxns);
  for (const auto &[key, recPtr] : detail::sortedEntries(agg)) {
    detail::writeHasPaidRow(w, key, *recPtr);
  }
}

} // namespace PhantomLedger::exporter::standard
