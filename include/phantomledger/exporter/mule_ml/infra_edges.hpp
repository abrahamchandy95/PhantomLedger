#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::mule_ml {

namespace encoding = ::PhantomLedger::encoding;
namespace entity = ::PhantomLedger::entity;
namespace exporter = ::PhantomLedger::exporter;
namespace hashing = ::PhantomLedger::hashing;
namespace network = ::PhantomLedger::network;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;
namespace txns = ::PhantomLedger::transactions;

namespace detail {

struct AccountItemKey {
  entity::Key account;
  std::string item;

  bool operator==(const AccountItemKey &) const noexcept = default;
};

struct AccountItemKeyHash {
  std::size_t operator()(const AccountItemKey &k) const noexcept {
    return hashing::make(k.account, k.item);
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
  std::vector<std::pair<AccountItemKey, EdgeAggregate>> out(edges.begin(),
                                                            edges.end());

  std::ranges::sort(out, [](const auto &a, const auto &b) noexcept {
    if (a.first.account != b.first.account) {
      return a.first.account < b.first.account;
    }
    return a.first.item < b.first.item;
  });

  return out;
}

inline void writeKeyCells(exporter::csv::Writer &w, const AccountItemKey &key) {
  w.cell(encoding::format(key.account).view()).cell(key.item);
}

inline void writeCountCell(exporter::csv::Writer &w, const EdgeAggregate &agg) {
  w.cell(agg.count);
}

inline void writeTimeRangeCells(exporter::csv::Writer &w,
                                const EdgeAggregate &agg) {
  w.cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(agg.firstTs)))
      .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(agg.lastTs)));
}

inline void writeEdgeRow(exporter::csv::Writer &w, const AccountItemKey &key,
                         const EdgeAggregate &agg) {
  writeKeyCells(w, key);
  writeCountCell(w, agg);
  writeTimeRangeCells(w, agg);
  w.endRow();
}

inline void emitSortedRows(exporter::csv::Writer &w, const EdgeMap &edges) {
  for (const auto &[key, agg] : sortedEdges(edges)) {
    writeEdgeRow(w, key, agg);
  }
}

} // namespace detail

inline void writeAccountDeviceRows(
    exporter::csv::Writer &w, std::span<const txns::Transaction> finalTxns,
    const synth::infra::devices::Output &devices,
    const std::unordered_map<entity::PersonId, std::vector<entity::Key>>
        &accountsByPerson) {

  detail::EdgeMap edges;
  edges.reserve(finalTxns.size() / 4 + 1);

  for (const auto &tx : finalTxns) {
    const auto deviceBuf =
        exporter::common::renderDeviceId(tx.session.deviceId);
    if (deviceBuf.empty()) {
      continue;
    }
    detail::AccountItemKey key{tx.source, std::string{deviceBuf.view()}};
    detail::touchEdge(edges[std::move(key)], tx.timestamp,
                      /*incrementCount=*/true);
  }

  for (const auto &usage : devices.usages) {
    const auto deviceBuf = exporter::common::renderDeviceId(usage.deviceId);
    if (deviceBuf.empty()) {
      continue;
    }
    const std::string deviceStr{deviceBuf.view()};
    const auto firstSeenEpoch = time_ns::toEpochSeconds(usage.firstSeen);
    const auto lastSeenEpoch = time_ns::toEpochSeconds(usage.lastSeen);

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
    exporter::csv::Writer &w, std::span<const txns::Transaction> finalTxns,
    const synth::infra::ips::Output &ips,
    const std::unordered_map<entity::PersonId, std::vector<entity::Key>>
        &accountsByPerson) {

  detail::EdgeMap edges;
  edges.reserve(finalTxns.size() / 4 + 1);

  for (const auto &tx : finalTxns) {
    const auto ipBuf = network::format(tx.session.ipAddress);
    if (ipBuf.empty()) {
      continue;
    }
    detail::AccountItemKey key{tx.source, std::string{ipBuf.view()}};
    detail::touchEdge(edges[std::move(key)], tx.timestamp,
                      /*incrementCount=*/true);
  }

  for (const auto &usage : ips.usages) {
    const auto ipBuf = network::format(usage.ipAddress);
    if (ipBuf.empty()) {
      continue;
    }
    const auto firstSeenEpoch = time_ns::toEpochSeconds(usage.firstSeen);
    const auto lastSeenEpoch = time_ns::toEpochSeconds(usage.lastSeen);

    const auto it = accountsByPerson.find(usage.personId);
    if (it == accountsByPerson.end()) {
      continue;
    }
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
