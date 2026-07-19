#pragma once

#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <cstddef>
#include <cstdio>
#include <sys/resource.h>

namespace PhantomLedger::transfers::legit::ledger::memlog {

[[nodiscard]] inline double peakRssMB() noexcept {
  struct rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
  return static_cast<double>(ru.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(ru.ru_maxrss) / 1024.0;
#endif
}

[[nodiscard]] constexpr double rowsMB(std::size_t rows) noexcept {
  return static_cast<double>(rows) *
         static_cast<double>(sizeof(transactions::Transaction)) /
         (1024.0 * 1024.0);
}

inline void log(const char *stage, const TxnStreams &streams) {
  const auto screened = streams.screened().size();
  const auto replay = streams.replayReady().size();
  const auto payday = streams.paydayInbound().size();
  std::fprintf(stderr,
               "[mem:b] %-22s peakRSS=%9.1f MB  screened=%zu (~%.0f MB)  "
               "replayReady=%zu (~%.0f MB)  paydayInbound=%zu (~%.0f MB)\n",
               stage, peakRssMB(), screened, rowsMB(screened), replay,
               rowsMB(replay), payday, rowsMB(payday));
}

inline void logPlain(const char *stage) {
  std::fprintf(stderr, "[mem:b] %-22s peakRSS=%9.1f MB\n", stage, peakRssMB());
}

} // namespace PhantomLedger::transfers::legit::ledger::memlog
