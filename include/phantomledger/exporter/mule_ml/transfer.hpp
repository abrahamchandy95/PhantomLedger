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

inline void writeTransferRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns) {
  namespace enc = ::PhantomLedger::encoding;
  namespace t = ::PhantomLedger::time;

  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    w.writeRow(detail::transferId(idx), enc::format(tx.source).view(),
               enc::format(tx.target).view(),
               primitives::utils::roundMoney(tx.amount),
               t::formatTimestamp(t::fromEpochSeconds(tx.timestamp)));
    ++idx;
  }
}

} // namespace PhantomLedger::exporter::mule_ml
