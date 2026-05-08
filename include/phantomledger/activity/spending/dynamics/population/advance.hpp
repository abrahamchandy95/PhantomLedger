#pragma once

#include "phantomledger/activity/spending/dynamics/dormancy/machine.hpp"
#include "phantomledger/activity/spending/dynamics/dormancy/state.hpp"
#include "phantomledger/activity/spending/dynamics/momentum/state.hpp"
#include "phantomledger/activity/spending/dynamics/momentum/update.hpp"
#include "phantomledger/activity/spending/dynamics/paycheck/boost.hpp"
#include "phantomledger/math/dormancy.hpp"
#include "phantomledger/math/momentus.hpp"
#include "phantomledger/math/paycheck.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::dynamics::population {

struct Drivers {
  math::momentum::Config momentum{};
  math::dormancy::Config dormancy{};
  math::paycheck::Config paycheck{};

  void validate(primitives::validate::Report &r) const {
    momentum.validate(r);
    dormancy.validate(r);
    paycheck.validate(r);
  }
};

inline constexpr Drivers kDefaultDrivers{};

class Cohort {
public:
  Cohort() = default;

  explicit Cohort(std::uint32_t personCount)
      : momentum_(personCount), dormancy_(personCount), paycheck_(personCount) {
  }

  [[nodiscard]] std::size_t size() const noexcept { return momentum_.size(); }

  [[nodiscard]] std::span<momentum::State> momentum() noexcept {
    return momentum_;
  }
  [[nodiscard]] std::span<dormancy::State> dormancy() noexcept {
    return dormancy_;
  }
  [[nodiscard]] std::span<paycheck::State> paycheck() noexcept {
    return paycheck_;
  }

  void advanceAll(random::Rng &rng, const Drivers &drivers,
                  std::span<const std::uint32_t> paydayPersonIndices,
                  std::span<const double> sensitivities,
                  std::span<double> outDailyMult) {
    std::fill(outDailyMult.begin(), outDailyMult.end(), 1.0);

    momentum::accumulate(rng, drivers.momentum, momentum_, outDailyMult);
    dormancy::accumulate(rng, drivers.dormancy, dormancy_, outDailyMult);

    paycheck::triggerForPaydays(drivers.paycheck, paycheck_, sensitivities,
                                paydayPersonIndices);
    paycheck::accumulate(paycheck_, outDailyMult);
  }

private:
  std::vector<momentum::State> momentum_;
  std::vector<dormancy::State> dormancy_;
  std::vector<paycheck::State> paycheck_;
};

} // namespace PhantomLedger::spending::dynamics::population
