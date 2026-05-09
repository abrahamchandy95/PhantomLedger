#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transfers/channels/government/benefits_emitter.hpp"

namespace PhantomLedger::transfers::government {

struct RetirementTerms {
  double eligibleP = 0.87;
  double median = 2071.0;
  double sigma = 0.30;
  double floor = 900.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("eligibleP", eligibleP); });
    r.check([&] { v::positive("median", median); });
    r.check([&] { v::nonNegative("sigma", sigma); });
    r.check([&] { v::nonNegative("floor", floor); });
  }
};

[[nodiscard]] inline bool isRetirementEligible(personas::Type t) noexcept {
  return t == personas::Type::retiree;
}

[[nodiscard]] inline BenefitProgram
retirementProgram(entity::Key counterparty) noexcept {
  return BenefitProgram{
      .eligible = &isRetirementEligible,
      .source = counterparty,
      .channel = channels::tag(channels::Government::socialSecurity),
  };
}

} // namespace PhantomLedger::transfers::government
