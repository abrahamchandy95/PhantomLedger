#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::standard::flow_agg {

namespace detail {

namespace tx_ns = ::PhantomLedger::transactions;
namespace ent = ::PhantomLedger::entity;
namespace enc = ::PhantomLedger::encoding;
namespace t = ::PhantomLedger::time;
namespace utils = ::PhantomLedger::primitives::utils;

inline constexpr std::int64_t kSecondsPerDay = 86'400;

inline constexpr int kDefaultBinDays = 14;

struct BinSpec {
  std::int64_t startEpoch = 0;
  std::int64_t binSeconds = kDefaultBinDays * kSecondsPerDay;
  std::size_t numBins = 1;
  int binDays = kDefaultBinDays;

  [[nodiscard]] std::size_t indexFor(std::int64_t ts) const noexcept {
    if (binSeconds <= 0 || ts <= startEpoch) {
      return 0;
    }
    const std::int64_t raw = (ts - startEpoch) / binSeconds;
    const auto idx = static_cast<std::size_t>(raw);
    return idx >= numBins ? numBins - 1 : idx;
  }
};

[[nodiscard]] inline BinSpec makeBinSpec(const t::Window &window, int binDays) {
  BinSpec spec;
  spec.binDays = binDays < 1 ? 1 : binDays;
  spec.binSeconds = static_cast<std::int64_t>(spec.binDays) * kSecondsPerDay;
  spec.startEpoch = t::toEpochSeconds(window.start);

  const std::int64_t spanDays = window.days > 0 ? window.days - 1 : 0;
  const std::int64_t bins = (spanDays / spec.binDays) + 1;
  spec.numBins = bins < 1 ? std::size_t{1} : static_cast<std::size_t>(bins);
  return spec;
}

struct BinCell {
  double amount = 0.0;
  std::uint32_t count = 0;
};

struct Aggregate {
  double totalAmount = 0.0;
  std::uint64_t txnCount = 0;
  std::int64_t firstTs = 0;
  std::int64_t lastTs = 0;
  std::unordered_map<std::uint32_t, BinCell> bins;
};

using AggregateMap =
    std::unordered_map<ent::KeyPair, Aggregate, ent::KeyPairHash>;

// Per-row accumulation, shared by the one-shot aggregation below and the
// windowed streaming exporter. Rows must arrive in corpus order so the
// floating-point sums stay bit-identical between the two paths.
inline void accumulate(AggregateMap &agg, const tx_ns::Transaction &tx,
                       const BinSpec &spec) {
  const ent::KeyPair key{tx.source, tx.target};
  auto &rec = agg[key];
  if (rec.txnCount == 0) {
    rec.firstTs = tx.timestamp;
    rec.lastTs = tx.timestamp;
  } else {
    if (tx.timestamp < rec.firstTs) {
      rec.firstTs = tx.timestamp;
    }
    if (tx.timestamp > rec.lastTs) {
      rec.lastTs = tx.timestamp;
    }
  }
  rec.totalAmount += tx.amount;
  ++rec.txnCount;

  const auto idx = static_cast<std::uint32_t>(spec.indexFor(tx.timestamp));
  auto &cell = rec.bins[idx];
  cell.amount += tx.amount;
  cell.count += 1U;
}

[[nodiscard]] inline AggregateMap
aggregate(std::span<const tx_ns::Transaction> txns, const BinSpec &spec) {
  AggregateMap agg;
  agg.reserve(txns.size() / 2 + 1);

  for (const auto &tx : txns) {
    accumulate(agg, tx, spec);
  }
  return agg;
}

inline void formatBins(const Aggregate &rec, const BinSpec &spec,
                       int moneyDecimals, std::string &amountOut,
                       std::string &countOut) {
  amountOut.clear();
  countOut.clear();
  amountOut.reserve(spec.numBins * 8);
  countOut.reserve(spec.numBins * 3);

  char abuf[64];
  char cbuf[32];
  for (std::size_t i = 0; i < spec.numBins; ++i) {
    if (i != 0) {
      amountOut.push_back(';');
      countOut.push_back(';');
    }
    double amount = 0.0;
    std::uint32_t count = 0;
    const auto it = rec.bins.find(static_cast<std::uint32_t>(i));
    if (it != rec.bins.end()) {
      amount = utils::roundMoney(it->second.amount);
      count = it->second.count;
    }
    const int an =
        std::snprintf(abuf, sizeof(abuf), "%.*f", moneyDecimals, amount);
    if (an > 0) {
      amountOut.append(abuf, static_cast<std::size_t>(an));
    }
    const int cn = std::snprintf(cbuf, sizeof(cbuf), "%u", count);
    if (cn > 0) {
      countOut.append(cbuf, static_cast<std::size_t>(cn));
    }
  }
}

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

inline void writeRow(::PhantomLedger::exporter::csv::Writer &w,
                     const ent::KeyPair &key, const Aggregate &rec,
                     const BinSpec &spec, int moneyDecimals,
                     std::string &amountScratch, std::string &countScratch) {
  const double spanDays = static_cast<double>(rec.lastTs - rec.firstTs) /
                          static_cast<double>(kSecondsPerDay);
  char spanBuf[32];
  const int spanLen = std::snprintf(spanBuf, sizeof(spanBuf), "%.4f", spanDays);
  const std::string_view spanView{
      spanBuf,
      spanLen > 0 ? static_cast<std::size_t>(spanLen) : std::size_t{0}};

  formatBins(rec, spec, moneyDecimals, amountScratch, countScratch);

  w.cell(enc::format(key.source).view())
      .cell(enc::format(key.target).view())
      .cell(utils::roundMoney(rec.totalAmount))
      .cell(rec.txnCount)
      .cell(t::formatTimestamp(t::fromEpochSeconds(rec.firstTs)))
      .cell(t::formatTimestamp(t::fromEpochSeconds(rec.lastTs)))
      .cell(spanView)
      .cell(static_cast<std::uint64_t>(spec.numBins))
      .cell(static_cast<std::int64_t>(spec.binDays))
      .cell(amountScratch)
      .cell(countScratch);
  w.endRow();
}

} // namespace detail

// Write side over an already-accumulated map (sorted, so output is
// independent of accumulation layout). The streaming exporter calls this
// at finish().
inline void
writeAccountFlowAggAggregates(::PhantomLedger::exporter::csv::Writer &w,
                              const detail::AggregateMap &agg,
                              const detail::BinSpec &spec,
                              int moneyDecimals = 2) {
  std::string amountScratch;
  std::string countScratch;
  for (const auto &[key, recPtr] : detail::sortedEntries(agg)) {
    detail::writeRow(w, key, *recPtr, spec, moneyDecimals, amountScratch,
                     countScratch);
  }
}

inline void writeAccountFlowAggRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns,
    const ::PhantomLedger::time::Window &window,
    int binDays = detail::kDefaultBinDays, int moneyDecimals = 2) {
  const auto spec = detail::makeBinSpec(window, binDays);
  const auto agg = detail::aggregate(finalTxns, spec);
  writeAccountFlowAggAggregates(w, agg, spec, moneyDecimals);
}

} // namespace PhantomLedger::exporter::standard::flow_agg
