#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <optional>

namespace PhantomLedger::transfers::credit_cards {

struct DisputeWindow {
  int refundMin = 1;
  int refundMax = 14;
  int chargebackMin = 7;
  int chargebackMax = 45;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::nonNegative("refundMin", refundMin); });
    r.check([&] { v::ge("refundMax", refundMax, refundMin); });
    r.check([&] { v::nonNegative("chargebackMin", chargebackMin); });
    r.check([&] { v::ge("chargebackMax", chargebackMax, chargebackMin); });
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

struct DisputeBehavior {
  DisputeWindow window{};
  DisputeRates rates{};

  void validate(primitives::validate::Report &r) const {
    window.validate(r);
    rates.validate(r);
  }
};

inline constexpr DisputeBehavior kDefaultDisputeBehavior{};

[[nodiscard]] std::optional<transactions::Transaction> sampleMerchantCredit(
    const DisputeWindow &window, const DisputeRates &rates, random::Rng &rng,
    const transactions::Transaction &purchase, time::TimePoint windowEndExcl,
    const transactions::Factory &factory);

} // namespace PhantomLedger::transfers::credit_cards
