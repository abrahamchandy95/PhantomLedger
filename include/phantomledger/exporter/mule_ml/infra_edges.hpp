#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/infra/synth/devices_output.hpp"
#include "phantomledger/infra/synth/ips_output.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/network/format.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::mule_ml {

namespace detail {

namespace common = ::PhantomLedger::exporter::common;
namespace enc = ::PhantomLedger::encoding;
namespace t = ::PhantomLedger::time;

struct AccountItemKey {
  ::PhantomLedger::entity::Key account;
  std::string item;

  bool operator==(const AccountItemKey &) const noexcept = default;
};

struct AccountItemKeyHash {
  std::size_t operator()(const AccountItemKey &k) const noexcept {
    const auto h1 = std::hash<::PhantomLedger::entity::Key>{}(k.account);
    const auto h2 = std::hash<std::string>{}(k.item);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
  }
};

struct EdgeAggregate {
  std::uint64_t count = 0;

  std::int64_t firstTs = 0;
  std::int64_t lastTs = 0;
};

using EdgeMap =
    std::unordered_map<AccountItemKey, EdgeAggregate, AccountItemKeyHash>;

inline void touchEdge(EdgeAggregate &agg, std::int64_t ts,
                      bool incrementCount) {
  if (incrementCount) {
    if (agg.count == 0) {
      agg.firstTs = ts;
      agg.lastTs = ts;
    } else {
      if (ts < agg.firstTs) {
        agg.firstTs = ts;
      } else if (ts > agg.lastTs) {
        agg.lastTs = ts;
      }
    }
    ++agg.count;
  } else {
    if (agg.count == 0) {
      agg.firstTs = ts;
      agg.lastTs = ts;
    } else {
      if (ts < agg.firstTs) {
        agg.firstTs = ts;
      }
      if (ts > agg.lastTs) {
        agg.lastTs = ts;
      }
    }
  }
}

[[nodiscard]] inline std::vector<std::pair<AccountItemKey, EdgeAggregate>>
sortedEdges(const EdgeMap &edges) {
  std::vector<std::pair<AccountItemKey, EdgeAggregate>> out;
  out.reserve(edges.size());
  for (const auto &kv : edges) {
    out.emplace_back(kv.first, kv.second);
  }
  std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) noexcept {
    if (a.first.account != b.first.account) {
      return a.first.account < b.first.account;
    }
    return a.first.item < b.first.item;
  });
  return out;
}

inline void writeKeyCells(::PhantomLedger::exporter::csv::Writer &w,
                          const AccountItemKey &key) {
  w.cell(enc::format(key.account).view()).cell(key.item);
}

inline void writeCountCell(::PhantomLedger::exporter::csv::Writer &w,
                           const EdgeAggregate &agg) {
  w.cell(agg.count);
}

inline void writeTimeRangeCells(::PhantomLedger::exporter::csv::Writer &w,
                                const EdgeAggregate &agg) {
  w.cell(t::formatTimestamp(t::fromEpochSeconds(agg.firstTs)))
      .cell(t::formatTimestamp(t::fromEpochSeconds(agg.lastTs)));
}

inline void writeEdgeRow(::PhantomLedger::exporter::csv::Writer &w,
                         const AccountItemKey &key, const EdgeAggregate &agg) {
  writeKeyCells(w, key);
  writeCountCell(w, agg);
  writeTimeRangeCells(w, agg);
  w.endRow();
}

inline void emitSortedRows(::PhantomLedger::exporter::csv::Writer &w,
                           const EdgeMap &edges) {
  for (const auto &[key, agg] : sortedEdges(edges)) {
    writeEdgeRow(w, key, agg);
  }
}

} // namespace detail

inline void writeAccountDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns,
    const ::PhantomLedger::infra::synth::devices::Output &devices,
    const std::unordered_map<::PhantomLedger::entity::PersonId,
                             std::vector<::PhantomLedger::entity::Key>>
        &accountsByPerson) {
  detail::EdgeMap edges;
  edges.reserve(finalTxns.size() / 4 + 1);

  for (const auto &tx : finalTxns) {
    const auto deviceBuf = detail::common::renderDeviceId(tx.session.deviceId);
    if (deviceBuf.empty()) {
      continue;
    }
    detail::AccountItemKey key{tx.source, std::string{deviceBuf.view()}};
    detail::touchEdge(edges[std::move(key)], tx.timestamp,
                      /*incrementCount=*/true);
  }

  for (const auto &usage : devices.usages) {
    const auto deviceBuf = detail::common::renderDeviceId(usage.deviceId);
    if (deviceBuf.empty()) {
      continue;
    }
    const std::string deviceStr{deviceBuf.view()};
    const auto firstSeenEpoch =
        ::PhantomLedger::time::toEpochSeconds(usage.firstSeen);
    const auto lastSeenEpoch =
        ::PhantomLedger::time::toEpochSeconds(usage.lastSeen);

    const auto it = accountsByPerson.find(usage.personId);
    if (it == accountsByPerson.end()) {
      continue;
    }
    for (const auto &accountKey : it->second) {
      detail::AccountItemKey key{accountKey, deviceStr};
      auto &agg = edges[key];
      detail::touchEdge(agg, firstSeenEpoch, /*incrementCount=*/false);
      detail::touchEdge(agg, lastSeenEpoch, /*incrementCount=*/false);
    }
  }

  detail::emitSortedRows(w, edges);
}

inline void writeAccountIpRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns,
    const ::PhantomLedger::infra::synth::ips::Output &ips,
    const std::unordered_map<::PhantomLedger::entity::PersonId,
                             std::vector<::PhantomLedger::entity::Key>>
        &accountsByPerson) {
  detail::EdgeMap edges;
  edges.reserve(finalTxns.size() / 4 + 1);

  for (const auto &tx : finalTxns) {
    // `network::format` is allocation-free; we materialize the owning
    // std::string for AccountItemKey::item exactly once per loop, at
    // the storage boundary.
    const auto ipBuf = ::PhantomLedger::network::format(tx.session.ipAddress);
    if (ipBuf.empty()) {
      continue;
    }
    detail::AccountItemKey key{tx.source, std::string{ipBuf.view()}};
    detail::touchEdge(edges[std::move(key)], tx.timestamp,
                      /*incrementCount=*/true);
  }

  for (const auto &usage : ips.usages) {
    const auto ipBuf = ::PhantomLedger::network::format(usage.ipAddress);
    if (ipBuf.empty()) {
      continue;
    }
    const auto firstSeenEpoch =
        ::PhantomLedger::time::toEpochSeconds(usage.firstSeen);
    const auto lastSeenEpoch =
        ::PhantomLedger::time::toEpochSeconds(usage.lastSeen);

    const auto it = accountsByPerson.find(usage.personId);
    if (it == accountsByPerson.end()) {
      continue;
    }
    // Materialize once per usage and share across all accounts of this
    // person — the inner loop copies the std::string into each
    // AccountItemKey, which is unavoidable storage.
    const auto ipStr = std::string{ipBuf.view()};
    for (const auto &accountKey : it->second) {
      detail::AccountItemKey key{accountKey, ipStr};
      auto &agg = edges[key];
      detail::touchEdge(agg, firstSeenEpoch, /*incrementCount=*/false);
      detail::touchEdge(agg, lastSeenEpoch, /*incrementCount=*/false);
    }
  }

  detail::emitSortedRows(w, edges);
}

} // namespace PhantomLedger::exporter::mule_ml
