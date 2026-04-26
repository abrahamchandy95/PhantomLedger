#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::transfers::credit_cards {

struct PaymentMixture {
  double payFull = 0.35;
  double payPartial = 0.30;
  double payMin = 0.25;
  double miss = 0.10;

  double partialAlpha = 2.0;
  double partialBeta = 5.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("payFull", payFull); });
    r.check([&] { v::unit("payPartial", payPartial); });
    r.check([&] { v::unit("payMin", payMin); });
    r.check([&] { v::unit("miss", miss); });
    r.check([&] { v::positive("partialAlpha", partialAlpha); });
    r.check([&] { v::positive("partialBeta", partialBeta); });
  }
};

struct PaymentTiming {
  double lateProbability = 0.08;

  int lateDaysMin = 1;
  int lateDaysMax = 20;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("lateProbability", lateProbability); });
    r.check([&] { v::nonNegative("lateDaysMin", lateDaysMin); });
    r.check([&] { v::ge("lateDaysMax", lateDaysMax, lateDaysMin); });
  }
};

struct DisputeRates {
  double refundProbability = 0.006;
  double chargebackProbability = 0.001;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("refundProbability", refundProbability); });
    r.check([&] { v::unit("chargebackProbability", chargebackProbability); });
  }
};

struct CardholderBehavior {
  PaymentMixture mixture{};
  PaymentTiming timing{};
  DisputeRates disputes{};

  void validate(primitives::validate::Report &r) const {
    mixture.validate(r);
    timing.validate(r);
    disputes.validate(r);
  }
};

inline constexpr CardholderBehavior kDefaultCardholderBehavior{};

} // namespace PhantomLedger::transfers::credit_cards
