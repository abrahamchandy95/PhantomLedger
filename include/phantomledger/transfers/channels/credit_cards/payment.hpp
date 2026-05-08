#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
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

struct PaymentBehavior {
  PaymentMixture mixture{};
  PaymentTiming timing{};

  void validate(primitives::validate::Report &r) const {
    mixture.validate(r);
    timing.validate(r);
  }
};

inline constexpr PaymentBehavior kDefaultPaymentBehavior{};

[[nodiscard]] double samplePaymentAmount(const PaymentMixture &mixture,
                                         random::Rng &rng, double statementAbs,
                                         double minimumDue);

[[nodiscard]] time::TimePoint samplePaymentTime(const PaymentTiming &timing,
                                                random::Rng &rng,
                                                time::TimePoint due,
                                                bool autopay);

} // namespace PhantomLedger::transfers::credit_cards
