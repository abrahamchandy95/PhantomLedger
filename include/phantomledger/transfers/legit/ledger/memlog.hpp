#pragma once

#include "phantomledger/diagnostics/logger.hpp"
#include "phantomledger/diagnostics/memory.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <cstddef>

/*
  Legit-build RAM observability
  Per-stage lines for the base-stream composition, reporting peak RSS and the
  resident size of each TxnStreams view. These lines are TOPIC-GATED: they
  once printed unconditionally on stderr under a literal `[mem:b]` prefix and
  are now silent by default like everything else, so enable them with
  `make run-mem`, or PL_LOG_LEVEL=info PL_LOG_TOPICS=mem.
 */

namespace PhantomLedger::transfers::legit::ledger::memlog {

/* Approximate in-memory footprint of `rows` retained transactions. */
[[nodiscard]] constexpr double rowsMB(std::size_t rows) noexcept {
  return static_cast<double>(rows) *
         static_cast<double>(sizeof(transactions::Transaction)) /
         (1024.0 * 1024.0);
}

/* One stage line with no stream inventory, for stages that retain no rows. */
inline void log(const char *stage) {
  PL_LOG_INFO(mem, "{:<22} peakRSS={:9.1f} MB", stage,
              ::PhantomLedger::diagnostics::memory::peakRssMB());
}

/* One stage line plus the row count and approximate size of each live view. */
inline void log(const char *stage, const TxnStreams &streams) {
  const auto screened = streams.screened().size();
  const auto replay = streams.replayReady().size();
  const auto payday = streams.paydayInbound().size();
  PL_LOG_INFO(mem,
              "{:<22} peakRSS={:9.1f} MB  screened={} (~{:.0f} MB)  "
              "replayReady={} (~{:.0f} MB)  paydayInbound={} (~{:.0f} MB)",
              stage, ::PhantomLedger::diagnostics::memory::peakRssMB(),
              screened, rowsMB(screened), replay, rowsMB(replay), payday,
              rowsMB(payday));
}

} // namespace PhantomLedger::transfers::legit::ledger::memlog
