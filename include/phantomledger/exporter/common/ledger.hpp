#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/transactions/network/format.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::exporter::common {

namespace detail {

namespace enc = ::PhantomLedger::encoding;
namespace t = ::PhantomLedger::time;
namespace net = ::PhantomLedger::network;
namespace ch = ::PhantomLedger::channels;
namespace tx_ns = ::PhantomLedger::transactions;

// src_acct, dst_acct
inline void writeRoutingCells(::PhantomLedger::exporter::csv::Writer &w,
                              const tx_ns::Transaction &tx) {
  w.cell(enc::format(tx.source).view()).cell(enc::format(tx.target).view());
}

// amount, ts
inline void writePaymentCells(::PhantomLedger::exporter::csv::Writer &w,
                              const tx_ns::Transaction &tx) {
  w.cell(tx.amount).cell(t::formatTimestamp(t::fromEpochSeconds(tx.timestamp)));
}

// is_fraud, ring_id
inline void writeFraudCells(::PhantomLedger::exporter::csv::Writer &w,
                            const tx_ns::Transaction &tx) {
  const std::uint32_t ringId = tx.fraud.ringId.value_or(0U);
  w.cell(static_cast<std::uint32_t>(tx.fraud.flag)).cell(ringId);
}

// device_id, ip_address, channel
inline void writeSessionCells(::PhantomLedger::exporter::csv::Writer &w,
                              const tx_ns::Transaction &tx) {
  w.cell(renderDeviceId(tx.session.deviceId).view())
      .cell(net::format(tx.session.ipAddress))
      .cell(ch::name(tx.session.channel));
}

inline void writeLedgerRow(::PhantomLedger::exporter::csv::Writer &w,
                           const tx_ns::Transaction &tx) {
  writeRoutingCells(w, tx);
  writePaymentCells(w, tx);
  writeFraudCells(w, tx);
  writeSessionCells(w, tx);
  w.endRow();
}

} // namespace detail

inline void writeLedgerRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns) {
  for (const auto &tx : finalTxns) {
    detail::writeLedgerRow(w, tx);
  }
}

} // namespace PhantomLedger::exporter::common
