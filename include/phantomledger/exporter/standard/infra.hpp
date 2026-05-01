#pragma once

#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/standard/internal/customer_id.hpp"
#include "phantomledger/infra/synth/devices_output.hpp"
#include "phantomledger/infra/synth/ips_output.hpp"
#include "phantomledger/infra/synth/types.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/network/format.hpp"

namespace PhantomLedger::exporter::standard {

inline void
writeDeviceRows(::PhantomLedger::exporter::csv::Writer &w,
                const ::PhantomLedger::infra::synth::devices::Output &devices) {
  for (const auto &record : devices.records) {
    w.writeRow(detail::renderDeviceId(record.identity),
               ::PhantomLedger::infra::synth::name(record.kind),
               record.flagged);
  }
}

inline void
writeIpAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                   const ::PhantomLedger::infra::synth::ips::Output &ips) {
  for (const auto &record : ips.records) {
    // network::format renders an Ipv4 to its dotted-quad string.
    w.writeRow(::PhantomLedger::network::format(record.address),
               record.blacklisted);
  }
}

inline void writeHasUsedRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices) {
  for (const auto &usage : devices.usages) {
    w.writeRow(detail::customerIdFor(usage.personId),
               detail::renderDeviceId(usage.deviceId),
               ::PhantomLedger::time::formatTimestamp(usage.firstSeen),
               ::PhantomLedger::time::formatTimestamp(usage.lastSeen));
  }
}

inline void
writeHasIpRows(::PhantomLedger::exporter::csv::Writer &w,
               const ::PhantomLedger::infra::synth::ips::Output &ips) {
  for (const auto &usage : ips.usages) {
    w.writeRow(detail::customerIdFor(usage.personId),
               ::PhantomLedger::network::format(usage.ipAddress),
               ::PhantomLedger::time::formatTimestamp(usage.firstSeen),
               ::PhantomLedger::time::formatTimestamp(usage.lastSeen));
  }
}

} // namespace PhantomLedger::exporter::standard
