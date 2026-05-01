#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cmath>
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

[[nodiscard]] inline double round2(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

} // namespace detail

inline void writeTransferRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns) {
  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    w.writeRow(
        detail::transferId(idx), ::PhantomLedger::encoding::format(tx.source),
        ::PhantomLedger::encoding::format(tx.target), detail::round2(tx.amount),
        ::PhantomLedger::time::formatTimestamp(
            ::PhantomLedger::time::fromEpochSeconds(tx.timestamp)));
    ++idx;
  }
}

} // namespace PhantomLedger::exporter::mule_ml
