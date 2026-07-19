#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/playbook.hpp"
#include "phantomledger/transfers/fraud/typologies/layering.hpp"
#include "phantomledger/transfers/fraud/typologies/structuring.hpp"

namespace PhantomLedger::transfers::fraud {

struct Behavior {
  PlaybookWeights playbooks{};
  typologies::layering::Rules layering{};
  typologies::structuring::Rules structuring{};
  camouflage::Rates camouflage{};

  void validate(primitives::validate::Report &r) const {
    playbooks.validate(r);
    layering.validate(r);
    structuring.validate(r);
    camouflage.validate(r);
  }
};

inline constexpr Behavior kDefaultBehavior{};

} // namespace PhantomLedger::transfers::fraud
