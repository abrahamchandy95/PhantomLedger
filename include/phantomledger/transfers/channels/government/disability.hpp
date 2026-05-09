#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transfers/channels/government/benefits_emitter.hpp"

namespace PhantomLedger::transfers::government {

struct DisabilityTerms {
  double eligibleP = 0.04;
  double median = 1630.0;
  double sigma = 0.25;
  double floor = 500.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("eligibleP", eligibleP); });
    r.check([&] { v::positive("median", median); });
    r.check([&] { v::nonNegative("sigma", sigma); });
    r.check([&] { v::nonNegative("floor", floor); });
  }
};

[[nodiscard]] inline bool isDisabilityEligible(personas::Type t) noexcept {
  return t != personas::Type::retiree && t != personas::Type::student;
}

[[nodiscard]] inline BenefitProgram
disabilityProgram(entity::Key counterparty) noexcept {
  return BenefitProgram{
      .eligible = &isDisabilityEligible,
      .source = counterparty,
      .channel = channels::tag(channels::Government::disability),
  };
}

} // namespace PhantomLedger::transfers::government
