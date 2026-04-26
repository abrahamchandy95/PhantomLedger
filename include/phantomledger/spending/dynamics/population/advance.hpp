#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/dynamics/config.hpp"
#include "phantomledger/spending/dynamics/dormancy/machine.hpp"
#include "phantomledger/spending/dynamics/dormancy/state.hpp"
#include "phantomledger/spending/dynamics/momentum/state.hpp"
#include "phantomledger/spending/dynamics/momentum/update.hpp"
#include "phantomledger/spending/dynamics/paycheck/boost.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::dynamics::population {

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

  void advanceAll(random::Rng &rng, const Config &cfg,
                  std::span<const std::uint32_t> paydayPersonIndices,
                  std::span<const double> sensitivities,
                  std::span<double> outMomentumMult,
                  std::span<double> outDormancyMult,
                  std::span<double> outPaycheckMult) {
    momentum::advanceAll(rng, cfg.momentum, momentum_, outMomentumMult);
    dormancy::advanceAll(rng, cfg.dormancy, dormancy_, outDormancyMult);

    paycheck::triggerForPaydays(cfg.paycheck, paycheck_, sensitivities,
                                paydayPersonIndices);
    paycheck::advanceAll(paycheck_, outPaycheckMult);
  }

private:
  std::vector<momentum::State> momentum_;
  std::vector<dormancy::State> dormancy_;
  std::vector<paycheck::State> paycheck_;
};

} // namespace PhantomLedger::spending::dynamics::population
