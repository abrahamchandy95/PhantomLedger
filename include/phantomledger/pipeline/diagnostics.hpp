#pragma once

#include "phantomledger/diagnostics/logger.hpp"
#include "phantomledger/diagnostics/memory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <utility>

// Pipeline RAM observability: the per-stage [mem] lines shared by the
// batch composition (simulate.cpp), the world build, and the windowed
// two-phase run. Owner directive (2026-07-19): the generator is silent
// by default — every line here prints only when the `mem` diagnostics
// topic is enabled (`make run-mem`, or PL_LOG_LEVEL=info
// PL_LOG_TOPICS=mem).

namespace PhantomLedger::pipeline::diagnostics {

// Approximate in-memory footprint of `rows` retained transactions.
[[nodiscard]] constexpr double rowsMB(std::size_t rows) noexcept {
  return static_cast<double>(rows) *
         static_cast<double>(
             sizeof(::PhantomLedger::transactions::Transaction)) /
         (1024.0 * 1024.0);
}

// One stage line: peak RSS so far, plus the row counts (and their
// approximate sizes) of whatever transaction streams the stage holds
// live. Pass an empty list for stages that retain no rows.
//   [mem] <stage> peakRSS=... MB  live:  name=rows (~MB) ...
inline void logStageMem(
    const char *stage,
    std::initializer_list<std::pair<const char *, std::size_t>> liveStreams) {
  namespace logging = ::PhantomLedger::diagnostics;
  if (!logging::Logger::instance().enabled(logging::Level::info,
                                           logging::Topic::mem)) {
    return;
  }

  std::fprintf(stderr, "[mem] %-18s peakRSS=%9.1f MB", stage,
               logging::memory::peakRssMB());
  if (liveStreams.size() != 0) {
    std::fprintf(stderr, "  live:");
    for (const auto &[name, rows] : liveStreams) {
      std::fprintf(stderr, "  %s=%zu (~%.0f MB)", name, rows, rowsMB(rows));
    }
  }
  std::fprintf(stderr, "\n");
}

} // namespace PhantomLedger::pipeline::diagnostics
