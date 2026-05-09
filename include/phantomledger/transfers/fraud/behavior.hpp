#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/typologies/layering.hpp"
#include "phantomledger/transfers/fraud/typologies/structuring.hpp"

namespace PhantomLedger::transfers::fraud {

struct Behavior {
  TypologyWeights typologies{};
  typologies::layering::Rules layering{};
  typologies::structuring::Rules structuring{};
  camouflage::Rates camouflage{};

  void validate(primitives::validate::Report &r) const {
    typologies.validate(r);
    layering.validate(r);
    structuring.validate(r);
    camouflage.validate(r);
  }
};

inline constexpr Behavior kDefaultBehavior{};

} // namespace PhantomLedger::transfers::fraud
