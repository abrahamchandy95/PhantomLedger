#pragma once

#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entropy/random/rng.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace PhantomLedger::inflows::selection {

using PersonId = entities::identifier::PersonId;

template <class CandidateFn, class BaseFn> class Selector {
public:
  Selector(CandidateFn candidate, BaseFn baseProbability)
      : candidate_(std::move(candidate)),
        baseProbability_(std::move(baseProbability)) {}

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

    std::uint32_t candidateCount = 0;
    double minBaseProbability = 1.0;

    for (PersonId person = 1; person <= personCount; ++person) {
      if (!candidate_(person)) {
        continue;
      }

      const double base = baseProbability_(person);
      ++candidateCount;
      minBaseProbability = std::min(minBaseProbability, base);
    }

    if (candidateCount == 0) {
      return 0.0;
    }

    const double hiMax = 1.0 / minBaseProbability;
    double lo = 0.0;
    double hi = hiMax;

    for (int i = 0; i < 40; ++i) {
      const double mid = (lo + hi) * 0.5;
      if (paidShare(personCount, mid) < target) {
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
