#pragma once

#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::transfers::credit_cards {

// This mixture applies to MANUAL payers only (~50% of cards):
// autopay-full (.40) and autopay-minimum (.10) cards bypass it in
// Session::draftPayment. The POPULATION full-payment share is
// .40 + .50 x payFull ~= .575, on the S-DCPC ~58% measured share —
// do not compare payFull alone to population statistics
// (card-behavior-2026-07 finding; docs/fraud_model_audit.md L-7).
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
