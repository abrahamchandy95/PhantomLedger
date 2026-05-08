#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace PhantomLedger::inflows::selection {

using PersonId = entity::PersonId;

// Selector<CandidateFn, BaseFn>
//
// Two callbacks:
//   - candidate(PersonId) -> bool:  is this person eligible at all?
//   - baseProbability(PersonId) -> double:  base p before scale

template <class CandidateFn, class BaseFn> class Selector {
public:
  Selector(CandidateFn candidate, BaseFn baseProbability)
      : candidate_(std::move(candidate)),
        baseProbability_(std::move(baseProbability)) {}

  // Kept for external callers that want the average share at a given
  // scale. Not used on the fitScale hot path.
  [[nodiscard]] double paidShare(std::uint32_t personCount,
                                 double scale) const {
    if (scale <= 0.0) {
      return 0.0;
    }

    double total = 0.0;
    std::uint32_t count = 0;

    for (PersonId person = 1; person <= personCount; ++person) {
      if (!candidate_(person)) {
        continue;
      }

      const double probability =
          std::clamp(baseProbability_(person) * scale, 0.0, 1.0);

      total += probability;
      ++count;
    }

    return count == 0 ? 0.0 : total / static_cast<double>(count);
  }

  [[nodiscard]] double fitScale(std::uint32_t personCount,
                                double targetFraction) const {
    const double target = std::clamp(targetFraction, 0.0, 1.0);
    if (target <= 0.0) {
      return 0.0;
    }

    // Single O(N) materialization pass. Pay the functor cost once.
    std::vector<double> baseProbs;
    baseProbs.reserve(personCount);
    double minBase = 1.0;

    for (PersonId person = 1; person <= personCount; ++person) {
      if (!candidate_(person)) {
        continue;
      }
      const double base = baseProbability_(person);
      baseProbs.push_back(base);
      if (base < minBase) {
        minBase = base;
      }
    }

    if (baseProbs.empty()) {
      return 0.0;
    }

    // Rearrange comparison to avoid a per-iteration division:

    const double countD = static_cast<double>(baseProbs.size());
    const double targetTotal = target * countD;

    // 30 iterations of bisection gives ~1e-9 relative resolution on
    const double hiMax = 1.0 / minBase;
    double lo = 0.0;
    double hi = hiMax;

    for (int i = 0; i < 30; ++i) {
      const double mid = (lo + hi) * 0.5;

      // Dense inner loop: mul, min, accumulate. Compiler can SIMD this.
      double total = 0.0;
      for (double b : baseProbs) {
        const double p = b * mid;
        total += (p > 1.0) ? 1.0 : p;
      }

      if (total < targetTotal) {
        lo = mid;
      } else {
        hi = mid;
      }
    }

    return hi;
  }

  [[nodiscard]] bool selected(random::Rng &rng, PersonId person,
                              double scale) const {
    if (!candidate_(person)) {
      return false;
    }

    const double probability =
        std::clamp(baseProbability_(person) * scale, 0.0, 1.0);

    return rng.coin(probability);
  }

private:
  CandidateFn candidate_;
  BaseFn baseProbability_;
};

template <class CandidateFn, class BaseFn>
[[nodiscard]] inline auto makeSelector(CandidateFn candidate,
                                       BaseFn baseProbability) {
  return Selector<CandidateFn, BaseFn>{std::move(candidate),
                                       std::move(baseProbability)};
}

} // namespace PhantomLedger::inflows::selection
