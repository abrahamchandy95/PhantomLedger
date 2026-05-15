#pragma once

#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/synth/infra/types.hpp"

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

} // namespace PhantomLedger::exporter::standard
