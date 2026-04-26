#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::transfers::credit_cards {

struct BillingPolicy {
  int graceDays = 25;

  double minPaymentPct = 0.02;
  double minPaymentDollars = 25.0;

  double lateFee = 32.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::ge("graceDays", graceDays, 1); });
    r.check([&] { v::unit("minPaymentPct", minPaymentPct); });
    r.check([&] { v::nonNegative("minPaymentDollars", minPaymentDollars); });
    r.check([&] { v::nonNegative("lateFee", lateFee); });
  }
};

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

struct IssuerPolicy {
  BillingPolicy billing{};
  DisputeWindow disputes{};

  void validate(primitives::validate::Report &r) const {
    billing.validate(r);
    disputes.validate(r);
  }
};

inline constexpr IssuerPolicy kDefaultIssuerPolicy{};

} // namespace PhantomLedger::transfers::credit_cards
