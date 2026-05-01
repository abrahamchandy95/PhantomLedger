#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/standard/internal/customer_id.hpp"
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

/// Sort the aggregation map's keys for deterministic output.
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

} // namespace detail

// ---------------------------------------------------------------------
// Account_Device.csv
// ---------------------------------------------------------------------

inline void writeAccountDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns,
    const ::PhantomLedger::infra::synth::devices::Output &devices,
    const std::unordered_map<::PhantomLedger::entity::PersonId,
                             std::vector<::PhantomLedger::entity::Key>>
        &accountsByPerson) {
  detail::EdgeMap edges;
  edges.reserve(finalTxns.size() / 4 + 1);

  // Phase 1 — txn aggregation.
  for (const auto &tx : finalTxns) {
    const auto deviceStr =
        ::PhantomLedger::exporter::standard::detail::renderDeviceId(
            tx.session.deviceId);
    if (deviceStr.empty()) {
      continue;
    }
    detail::AccountItemKey key{tx.source, deviceStr};
    detail::touchEdge(edges[std::move(key)], tx.timestamp,
                      /*incrementCount=*/true);
  }

  for (const auto &usage : devices.usages) {
    const auto deviceStr =
        ::PhantomLedger::exporter::standard::detail::renderDeviceId(
            usage.deviceId);
    if (deviceStr.empty()) {
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
    for (const auto &accountKey : it->second) {
      detail::AccountItemKey key{accountKey, deviceStr};
      auto &agg = edges[key];
      detail::touchEdge(agg, firstSeenEpoch, /*incrementCount=*/false);
      detail::touchEdge(agg, lastSeenEpoch, /*incrementCount=*/false);
    }
  }

  // Phase 3 — emit sorted rows.
  for (const auto &[key, agg] : detail::sortedEdges(edges)) {
    w.writeRow(::PhantomLedger::encoding::format(key.account), key.item,
               agg.count,
               ::PhantomLedger::time::formatTimestamp(
                   ::PhantomLedger::time::fromEpochSeconds(agg.firstTs)),
               ::PhantomLedger::time::formatTimestamp(
                   ::PhantomLedger::time::fromEpochSeconds(agg.lastTs)));
  }
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
    const auto ipStr = ::PhantomLedger::network::format(tx.session.ipAddress);
    if (ipStr.empty()) {
      continue;
    }
    detail::AccountItemKey key{tx.source, ipStr};
    detail::touchEdge(edges[std::move(key)], tx.timestamp,
                      /*incrementCount=*/true);
  }

  for (const auto &usage : ips.usages) {
    const auto ipStr = ::PhantomLedger::network::format(usage.ipAddress);
    if (ipStr.empty()) {
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
    for (const auto &accountKey : it->second) {
      detail::AccountItemKey key{accountKey, ipStr};
      auto &agg = edges[key];
      detail::touchEdge(agg, firstSeenEpoch, /*incrementCount=*/false);
      detail::touchEdge(agg, lastSeenEpoch, /*incrementCount=*/false);
    }
  }

  for (const auto &[key, agg] : detail::sortedEdges(edges)) {
    w.writeRow(::PhantomLedger::encoding::format(key.account), key.item,
               agg.count,
               ::PhantomLedger::time::formatTimestamp(
                   ::PhantomLedger::time::fromEpochSeconds(agg.firstTs)),
               ::PhantomLedger::time::formatTimestamp(
                   ::PhantomLedger::time::fromEpochSeconds(agg.lastTs)));
  }
}

} // namespace PhantomLedger::exporter::mule_ml
