#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/resource.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

// Process- and machine-memory probes: the two facts the RAM
// diagnostics need from the operating system. Consumers format and
// gate their own reporting (see pipeline/diagnostics.hpp for the [mem]
// stage lines; the spending planner emits the pre-flight estimate and
// the swap-risk warning).

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

// Physical RAM of this machine, in MiB; 0.0 when the OS will not say
// (callers treat that as "budget checks disarmed", never as zero RAM).
[[nodiscard]] inline double physicalRamMB() noexcept {
#if defined(__APPLE__)
  std::uint64_t bytes = 0;
  std::size_t len = sizeof(bytes);
  if (sysctlbyname("hw.memsize", &bytes, &len, nullptr, 0) != 0) {
    return 0.0;
  }
  return static_cast<double>(bytes) / (1024.0 * 1024.0);
#else
  const long pages = sysconf(_SC_PHYS_PAGES);
  const long pageSize = sysconf(_SC_PAGE_SIZE);
  if (pages <= 0 || pageSize <= 0) {
    return 0.0;
  }
  return static_cast<double>(pages) * static_cast<double>(pageSize) /
         (1024.0 * 1024.0);
#endif
}

} // namespace PhantomLedger::diagnostics::memory
