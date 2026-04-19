#pragma once

#include "phantomledger/math/sampling.hpp"
#include "phantomledger/random/rng.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::accounts {

[[nodiscard]] inline std::vector<int> counts(random::Rng &rng, int people,
                                             int maxPerPerson) {
  std::vector<int> out(static_cast<std::size_t>(people));

  if (maxPerPerson <= 1) {
    std::fill(out.begin(), out.end(), 1);
    return out;
  }

  const std::int64_t trials = static_cast<std::int64_t>(maxPerPerson - 1);
  for (auto &count : out) {
    count = static_cast<int>(math::binomial(rng, trials, 0.25)) + 1;
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::accounts
