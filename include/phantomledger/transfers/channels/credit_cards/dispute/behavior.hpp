#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::transfers::credit_cards {

/// Day-offset windows for post-purchase merchant credits.
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

/// Per-purchase probabilities of a refund or a chargeback occurring.
struct DisputeRates {
  double refundProbability = 0.006;
  double chargebackProbability = 0.001;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("refundProbability", refundProbability); });
    r.check([&] { v::unit("chargebackProbability", chargebackProbability); });
  }
};

/// Aggregate dispute behavior
struct DisputeBehavior {
  DisputeWindow window{};
  DisputeRates rates{};

  void validate(primitives::validate::Report &r) const {
    window.validate(r);
    rates.validate(r);
  }
};

inline constexpr DisputeBehavior kDefaultDisputeBehavior{};

} // namespace PhantomLedger::transfers::credit_cards
