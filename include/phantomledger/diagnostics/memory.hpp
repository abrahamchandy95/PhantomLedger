#pragma once

#include <sys/resource.h>

// Process-memory probe: the one fact the RAM diagnostics need from the
// operating system. Consumers format and gate their own reporting (see
// pipeline/diagnostics.hpp for the [mem] stage lines).

namespace PhantomLedger::diagnostics::memory {

// Peak resident set size of this process, in MiB. ru_maxrss is bytes
// on macOS and KiB on Linux.
[[nodiscard]] inline double peakRssMB() noexcept {
  struct rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
  return static_cast<double>(ru.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(ru.ru_maxrss) / 1024.0;
#endif
}

} // namespace PhantomLedger::diagnostics::memory
