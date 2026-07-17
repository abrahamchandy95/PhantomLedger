#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdio>
#include <span>
#include <string>

namespace PhantomLedger::exporter::mule_ml {

namespace detail {

[[nodiscard]] inline std::string transferId(std::size_t index1) {
  char buf[14];

  std::snprintf(buf, sizeof(buf), "T%012zu", index1);
  return std::string{buf};
}

} // namespace detail

// Streaming form: `nextIndex1` carries the 1-based transfer id across
// batches, so the windowed sink emits identical ids to the one-shot path.
inline void writeTransferRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns,
    std::size_t &nextIndex1) {
  namespace enc = ::PhantomLedger::encoding;
  namespace t = ::PhantomLedger::time;

  for (const auto &tx : finalTxns) {
    w.writeRow(detail::transferId(nextIndex1), enc::format(tx.source).view(),
               enc::format(tx.target).view(),
               primitives::utils::roundMoney(tx.amount),
               t::formatTimestamp(t::fromEpochSeconds(tx.timestamp)));
    ++nextIndex1;
  }
}

inline void writeTransferRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns) {
  std::size_t idx = 1;
  writeTransferRows(w, finalTxns, idx);
}

} // namespace PhantomLedger::exporter::mule_ml
