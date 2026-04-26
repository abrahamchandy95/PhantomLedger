#pragma once

#include "phantomledger/math/dormancy.hpp"
#include "phantomledger/math/evolution.hpp"
#include "phantomledger/math/momentus.hpp"
#include "phantomledger/math/paycheck.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::spending::dynamics {

struct Config {
  math::momentum::Config momentum{};
  math::dormancy::Config dormancy{};
  math::paycheck::Config paycheck{};
  math::evolution::Config evolution{};

  void validate(primitives::validate::Report &r) const {
    momentum.validate(r);
    dormancy.validate(r);
    paycheck.validate(r);
    evolution.validate(r);
  }
};

inline constexpr Config kDefaultConfig{};

} // namespace PhantomLedger::spending::dynamics
