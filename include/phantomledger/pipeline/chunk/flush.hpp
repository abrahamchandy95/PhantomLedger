#pragma once

#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace PhantomLedger::pipeline::chunk {

template <Sink S>
std::uint64_t
flushUnpartitioned(const Schedule &schedule,
                   std::span<const transactions::Transaction> rows, S &sink) {
  if (schedule.size() != 1) {
    throw std::logic_error(
        "flushUnpartitioned: schedule must contain exactly one span; "
        "use Schedule::unpartitioned");
  }
  const Span &span = *schedule.begin();
  sink.beginSpan(span);
  sink.append(rows);
  sink.endSpan(span);
  sink.finish();
  return sink.rowsWritten();
}

template <Sink S, class OnSpanDone>
std::uint64_t flushPartitioned(const Schedule &schedule,
                               std::span<const transactions::Transaction> rows,
                               S &sink, OnSpanDone &&onSpanDone) {
  if (schedule.empty()) {
    throw std::logic_error("flushPartitioned: empty schedule");
  }
  const bool sorted = std::is_sorted(
      rows.begin(), rows.end(),
      [](const auto &a, const auto &b) { return a.timestamp < b.timestamp; });
  if (!sorted) {
    throw std::logic_error(
        "flushPartitioned: rows must be timestamp-nondecreasing "
        "(replay order)");
  }

  std::size_t offset = 0;
  const std::size_t spanCount = schedule.size();
  for (std::size_t k = 0; k < spanCount; ++k) {
    const Span &span = schedule[k];
    std::size_t last = rows.size();
    if (k + 1 < spanCount) {
      const auto boundSec =
          span.activeWindow.endExcl().time_since_epoch().count();
      const auto it = std::lower_bound(
          rows.begin() + static_cast<std::ptrdiff_t>(offset), rows.end(),
          boundSec, [](const transactions::Transaction &tx, std::int64_t s) {
            return tx.timestamp < s;
          });
      last = static_cast<std::size_t>(it - rows.begin());
    }

    sink.beginSpan(span);
    if (last > offset) {
      sink.append(rows.subspan(offset, last - offset));
    }
    sink.endSpan(span);
    onSpanDone(span, static_cast<std::uint64_t>(last - offset));
    offset = last;
  }

  sink.finish();
  return sink.rowsWritten();
}

template <Sink S>
std::uint64_t flushPartitioned(const Schedule &schedule,
                               std::span<const transactions::Transaction> rows,
                               S &sink) {
  return flushPartitioned(schedule, rows, sink,
                          [](const Span &, std::uint64_t) {});
}

} // namespace PhantomLedger::pipeline::chunk
