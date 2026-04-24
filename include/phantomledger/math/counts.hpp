#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/probability/distributions/gamma.hpp"
#include "phantomledger/probability/distributions/poisson.hpp"

#include <algorithm>

namespace PhantomLedger::math::counts {

struct Rates {
  double gammaShapeK = 1.5;
  double weekendMultiplier = 0.8;
  int maxTxnsPerAccountPerDay = 0; // 0 = no cap

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::gt("gammaShapeK", gammaShapeK, 0.0); });
    r.check([&] { v::nonNegative("weekendMultiplier", weekendMultiplier); });
    r.check([&] {
      v::nonNegative("maxTxnsPerAccountPerDay", maxTxnsPerAccountPerDay);
    });
  }
};

inline constexpr Rates kDefaultRates{};

[[nodiscard]] inline double
weekdayMultiplier(time::TimePoint dayStart,
                  const Rates &rates = kDefaultRates) {
  return time::isWeekend(dayStart) ? rates.weekendMultiplier : 1.0;
}

[[nodiscard]] inline int gammaPoisson(random::Rng &rng, double baseRate,
                                      const Rates &rates = kDefaultRates) {
  if (baseRate <= 0.0) {
    return 0;
  }

  const double scale = std::max(1e-12, baseRate / rates.gammaShapeK);
  const double lamDay =
      probability::distributions::gamma(rng, rates.gammaShapeK, scale);
  const auto n =
      static_cast<int>(probability::distributions::poisson(rng, lamDay));

  const int cap = rates.maxTxnsPerAccountPerDay;
  return (cap > 0 && n > cap) ? cap : n;
}

} // namespace PhantomLedger::math::counts
