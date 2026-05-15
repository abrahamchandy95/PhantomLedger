#pragma once

#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/entities/infra/devices_output.hpp"
#include "phantomledger/synth/entities/infra/ips_output.hpp"
#include "phantomledger/synth/entities/infra/types.hpp"

namespace PhantomLedger::exporter::standard {

namespace common = ::PhantomLedger::exporter::common;

inline void
writeDeviceRows(::PhantomLedger::exporter::csv::Writer &w,
                const ::PhantomLedger::infra::synth::devices::Output &devices) {
  for (const auto &record : devices.records) {
    w.writeRow(common::renderDeviceId(record.identity).view(),
               ::PhantomLedger::infra::synth::name(record.kind),
               record.flagged);
  }
}

inline void
writeIpAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                   const ::PhantomLedger::infra::synth::ips::Output &ips) {
  for (const auto &record : ips.records) {
    w.writeRow(::PhantomLedger::network::format(record.address),
               record.blacklisted);
  }
}

inline void writeHasUsedRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices) {
  for (const auto &usage : devices.usages) {
    w.writeRow(common::renderCustomerId(usage.personId).view(),
               common::renderDeviceId(usage.deviceId).view(),
               ::PhantomLedger::time::formatTimestamp(usage.firstSeen),
               ::PhantomLedger::time::formatTimestamp(usage.lastSeen));
  }
}

inline void
writeHasIpRows(::PhantomLedger::exporter::csv::Writer &w,
               const ::PhantomLedger::infra::synth::ips::Output &ips) {
  for (const auto &usage : ips.usages) {
    w.writeRow(common::renderCustomerId(usage.personId).view(),
               ::PhantomLedger::network::format(usage.ipAddress),
               ::PhantomLedger::time::formatTimestamp(usage.firstSeen),
               ::PhantomLedger::time::formatTimestamp(usage.lastSeen));
  }
}

} // namespace PhantomLedger::exporter::standard
