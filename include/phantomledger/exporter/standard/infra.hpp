#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/synth/infra/types.hpp"
#include "phantomledger/synth/pii/membership.hpp"

#include <string>
#include <string_view>
#include <unordered_set>

namespace PhantomLedger::exporter::standard {

namespace exporter = ::PhantomLedger::exporter;
namespace network = ::PhantomLedger::network;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;

inline void writeDeviceRows(exporter::csv::Writer &w,
                            const synth::infra::devices::Output &devices) {
  for (const auto &record : devices.records) {
    w.writeRow(exporter::common::renderDeviceId(record.identity).view(),
               synth::infra::name(record.kind), record.flagged);
  }
}

inline void writeIpAddressRows(exporter::csv::Writer &w,
                               const synth::infra::ips::Output &ips) {
  for (const auto &record : ips.records) {
    w.writeRow(network::format(record.address), record.blacklisted);
  }
}

inline void writeHasUsedRows(exporter::csv::Writer &w,
                             const synth::infra::devices::Output &devices) {
  for (const auto &usage : devices.usages) {
    w.writeRow(exporter::common::renderCustomerId(usage.personId).view(),
               exporter::common::renderDeviceId(usage.deviceId).view(),
               time_ns::formatTimestamp(usage.firstSeen),
               time_ns::formatTimestamp(usage.lastSeen));
  }
}

inline void writeHasIpRows(exporter::csv::Writer &w,
                           const synth::infra::ips::Output &ips) {
  for (const auto &usage : ips.usages) {
    w.writeRow(exporter::common::renderCustomerId(usage.personId).view(),
               network::format(usage.ipAddress),
               time_ns::formatTimestamp(usage.firstSeen),
               time_ns::formatTimestamp(usage.lastSeen));
  }
}

inline void writeHasDeviceEdgeRows(exporter::csv::Writer &w,
                                   const synth::infra::devices::Output &devices,
                                   const synth::pii::Membership &membership) {
  namespace ent = ::PhantomLedger::entity;
  std::unordered_set<std::string> seen;
  for (const auto &usage : devices.usages) {
    if (usage.personId == ent::invalidPerson) {
      continue;
    }
    if (!membership.activeAt(usage.personId,
                             time_ns::toEpochSeconds(usage.lastSeen))) {
      continue;
    }
    const auto custId = exporter::common::renderCustomerId(usage.personId);
    const auto devId = exporter::common::renderDeviceId(usage.deviceId);
    std::string key;
    key.reserve(custId.view().size() + 1 + devId.view().size());
    key.append(custId.view());
    key.push_back('|');
    key.append(devId.view());
    if (seen.insert(key).second) {
      w.writeRow(custId.view(), devId.view());
    }
  }
}

inline void writeHasIpEdgeRows(exporter::csv::Writer &w,
                               const synth::infra::ips::Output &ips,
                               const synth::pii::Membership &membership) {
  namespace ent = ::PhantomLedger::entity;
  std::unordered_set<std::string> seen;
  for (const auto &usage : ips.usages) {
    if (usage.personId == ent::invalidPerson) {
      continue;
    }
    if (!membership.activeAt(usage.personId,
                             time_ns::toEpochSeconds(usage.lastSeen))) {
      continue;
    }
    const auto custId = exporter::common::renderCustomerId(usage.personId);
    const auto ipBuf = network::format(usage.ipAddress);
    const auto ipView = ipBuf.view();
    std::string key;
    key.reserve(custId.view().size() + 1 + ipView.size());
    key.append(custId.view());
    key.push_back('|');
    key.append(ipView);
    if (seen.insert(key).second) {
      w.writeRow(custId.view(), ipView);
    }
  }
}

} // namespace PhantomLedger::exporter::standard
