#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/family/graph_config.hpp"

namespace PhantomLedger::transfers::family {

// -----------------------------------------------------------------------------
// Allowances — parent → student-child weekly/biweekly schedule.
// -----------------------------------------------------------------------------

struct Allowances {
  bool enabled = true;
  double weeklyP = 0.60;
  double paretoXm = 15.0;
  double paretoAlpha = 2.2;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("weeklyP", weeklyP, 0.0, 1.0); });
    r.check([&] { v::gt("paretoXm", paretoXm, 0.0); });
    r.check([&] { v::gt("paretoAlpha", paretoAlpha, 0.0); });
  }
};

// -----------------------------------------------------------------------------
// Tuition — parent → education-merchant installment schedule.
// -----------------------------------------------------------------------------

struct Tuition {
  bool enabled = true;
  double p = 0.55;
  int instMin = 4;
  int instMax = 5;
  double mu = 8.7;
  double sigma = 0.35;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("p", p, 0.0, 1.0); });
    r.check([&] { v::ge("instMin", instMin, 1); });
    r.check([&] { v::ge("instMax", instMax, instMin); });
    r.check([&] { v::nonNegative("sigma", sigma); });
  }
};

// -----------------------------------------------------------------------------
// Spouses — intra-couple recurring transfer process.
// -----------------------------------------------------------------------------
struct Spouses {
  bool enabled = true;
  double separateAccountsP = 0.60;
  int txnsPerMonthMin = 2;
  int txnsPerMonthMax = 6;
  double breadwinnerFlowP = 0.65;
  double transferMedian = 85.0;
  double transferSigma = 0.9;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check(
        [&] { v::between("separateAccountsP", separateAccountsP, 0.0, 1.0); });
    r.check([&] { v::ge("txnsPerMonthMin", txnsPerMonthMin, 1); });
    r.check(
        [&] { v::ge("txnsPerMonthMax", txnsPerMonthMax, txnsPerMonthMin); });
    r.check(
        [&] { v::between("breadwinnerFlowP", breadwinnerFlowP, 0.0, 1.0); });
    r.check([&] { v::gt("transferMedian", transferMedian, 0.0); });
    r.check([&] { v::nonNegative("transferSigma", transferSigma); });
  }
};

// -----------------------------------------------------------------------------
// ParentGifts — working-parent → adult-child gift process.
// -----------------------------------------------------------------------------

struct ParentGifts {
  bool enabled = true;
  double p = 0.12;
  double paretoXm = 75.0;
  double paretoAlpha = 1.6;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("p", p, 0.0, 1.0); });
    r.check([&] { v::gt("paretoXm", paretoXm, 0.0); });
    r.check([&] { v::gt("paretoAlpha", paretoAlpha, 0.0); });
  }
};

// -----------------------------------------------------------------------------
// SiblingTransfers — irregular intra-household sibling transfers.
// -----------------------------------------------------------------------------

struct SiblingTransfers {
  bool enabled = true;
  double activeP = 0.15;
  double monthlyP = 0.18;
  double median = 120.0;
  double sigma = 0.90;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("activeP", activeP, 0.0, 1.0); });
    r.check([&] { v::between("monthlyP", monthlyP, 0.0, 1.0); });
    r.check([&] { v::gt("median", median, 0.0); });
    r.check([&] { v::nonNegative("sigma", sigma); });
  }
};

// -----------------------------------------------------------------------------
// GrandparentGifts — retiree → grandchild monthly process.
// -----------------------------------------------------------------------------

struct GrandparentGifts {
  bool enabled = true;
  double p = 0.08;
  double median = 150.0;
  double sigma = 0.70;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("p", p, 0.0, 1.0); });
    r.check([&] { v::gt("median", median, 0.0); });
    r.check([&] { v::nonNegative("sigma", sigma); });
  }
};

// -----------------------------------------------------------------------------
// Inheritance — rare retiree → heirs lump-sum.
// -----------------------------------------------------------------------------

struct Inheritance {
  bool enabled = true;
  double eventP = 0.0015;
  double median = 25000.0;
  double sigma = 1.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("eventP", eventP, 0.0, 1.0); });
    r.check([&] { v::gt("median", median, 0.0); });
    r.check([&] { v::nonNegative("sigma", sigma); });
  }
};

// -----------------------------------------------------------------------------
// Routing — cross-bank counterparty resolution for family payers/payees.
// -----------------------------------------------------------------------------

struct Routing {
  double externalP = 0.18;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("externalP", externalP, 0.0, 1.0); });
  }
};

// -----------------------------------------------------------------------------
// TransferConfig — bundle consumed by
// `routines::relatives::generateFamilyTxns`.
// -----------------------------------------------------------------------------

struct TransferConfig {
  Allowances allowances{};
  Tuition tuition{};
  RetireeSupport retireeSupport{};
  Spouses spouses{};
  ParentGifts parentGifts{};
  SiblingTransfers siblingTransfers{};
  GrandparentGifts grandparentGifts{};
  Inheritance inheritance{};
  Routing routing{};

  void validate(primitives::validate::Report &r) const {
    allowances.validate(r);
    tuition.validate(r);
    retireeSupport.validate(r);
    spouses.validate(r);
    parentGifts.validate(r);
    siblingTransfers.validate(r);
    grandparentGifts.validate(r);
    inheritance.validate(r);
    routing.validate(r);
  }
};

inline constexpr Allowances kDefaultAllowances{};
inline constexpr Tuition kDefaultTuition{};
inline constexpr Spouses kDefaultSpouses{};
inline constexpr ParentGifts kDefaultParentGifts{};
inline constexpr SiblingTransfers kDefaultSiblingTransfers{};
inline constexpr GrandparentGifts kDefaultGrandparentGifts{};
inline constexpr Inheritance kDefaultInheritance{};
inline constexpr Routing kDefaultRouting{};
inline constexpr TransferConfig kDefaultTransferConfig{};

} // namespace PhantomLedger::transfers::family
