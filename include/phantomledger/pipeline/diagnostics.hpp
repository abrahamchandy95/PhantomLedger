#pragma once

#include "phantomledger/diagnostics/logger.hpp"
#include "phantomledger/diagnostics/memory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <format>
#include <initializer_list>
#include <string>
#include <utility>

/*
  Pipeline RAM observability
  The per-stage RAM lines shared by the batch composition, the world build and
  the windowed two-phase run. The generator is silent by default, so these
  print only under the `mem` topic (`make run-mem`, or PL_LOG_LEVEL=info
  PL_LOG_TOPICS=mem).
 */

namespace PhantomLedger::pipeline::diagnostics {

/* Approximate in-memory footprint of `rows` retained transactions. */
[[nodiscard]] constexpr double rowsMB(std::size_t rows) noexcept {
  return static_cast<double>(rows) *
         static_cast<double>(
             sizeof(::PhantomLedger::transactions::Transaction)) /
         (1024.0 * 1024.0);
}

/* One stage line: peak RSS so far, plus the row counts and approximate sizes
 * of whatever transaction streams the stage holds live. Pass an empty list
 * for stages that retain no rows.
 *   <stage> peakRSS=... MB  live:  name=rows (~MB) ... */
inline void logStageMem(
    const char *stage,
    std::initializer_list<std::pair<const char *, std::size_t>> liveStreams) {
  namespace logging = ::PhantomLedger::diagnostics;
  if (!logging::Logger::instance().enabled(logging::Level::info,
                                           logging::Topic::mem)) {
    return;
  }

  std::string live;
  if (liveStreams.size() != 0) {
    live = "  live:";
    for (const auto &[name, rows] : liveStreams) {
      live += std::format("  {}={} (~{:.0f} MB)", name, rows, rowsMB(rows));
    }
  }
  PL_LOG_INFO(mem, "{:<18} peakRSS={:9.1f} MB{}", stage,
              logging::memory::peakRssMB(), live);
}

} // namespace PhantomLedger::pipeline::diagnostics
