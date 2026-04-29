#pragma once

#include "phantomledger/entropy/random/rng.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::relationships::social {

struct Communities {
  std::vector<std::uint32_t> starts;   // size == count
  std::vector<std::uint32_t> ends;     // size == count
  std::vector<std::uint32_t> memberOf; // size == personCount

  [[nodiscard]] std::uint32_t count() const noexcept {
    return static_cast<std::uint32_t>(starts.size());
  }

  [[nodiscard]] std::uint32_t blockSize(std::uint32_t b) const noexcept {
    return ends[b] - starts[b];
  }
};

[[nodiscard]] Communities buildCommunities(random::Rng &rng,
                                           std::uint32_t personCount, int cMin,
                                           int cMax);

} // namespace PhantomLedger::relationships::social
