#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::transfers::family {

// -----------------------------------------------------------------------------
// Households — physical residence partitioning.
// -----------------------------------------------------------------------------

/// Drives the household-size distribution.

struct Households {
  double singleP = 0.29;
  double zipfAlpha = 2.2;
  int maxSize = 6;
  double spouseP = 0.62;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("singleP", singleP, 0.0, 1.0); });
    r.check([&] { v::gt("zipfAlpha", zipfAlpha, 0.0); });
    r.check([&] { v::ge("maxSize", maxSize, 2); });
    r.check([&] { v::between("spouseP", spouseP, 0.0, 1.0); });
  }
};

// Dependents — student-child parent-link policy.
struct Dependents {
  double studentDependentP = 0.65;
  double studentCoresidesP = 0.35;
  double twoParentP = 0.70;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check(
        [&] { v::between("studentDependentP", studentDependentP, 0.0, 1.0); });
    r.check(
        [&] { v::between("studentCoresidesP", studentCoresidesP, 0.0, 1.0); });
    r.check([&] { v::between("twoParentP", twoParentP, 0.0, 1.0); });
  }
};

// RetireeSupport — cross-household financial-support tie policy.
struct RetireeSupport {
  bool enabled = true;

  double hasChildP = 0.35;

  double supportP = 0.35;

  double coresidesP = 0.12;

  double paretoXm = 25.0;
  double paretoAlpha = 2.4;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("hasChildP", hasChildP, 0.0, 1.0); });
    r.check([&] { v::between("supportP", supportP, 0.0, 1.0); });
    r.check([&] { v::between("coresidesP", coresidesP, 0.0, 1.0); });
    r.check([&] { v::gt("paretoXm", paretoXm, 0.0); });
    r.check([&] { v::gt("paretoAlpha", paretoAlpha, 0.0); });
  }
};

// -----------------------------------------------------------------------------
// GraphConfig — bundle consumed by the family-graph orchestrator.
// -----------------------------------------------------------------------------

struct GraphConfig {
  Households households{};
  Dependents dependents{};
  RetireeSupport retireeSupport{};

  void validate(primitives::validate::Report &r) const {
    households.validate(r);
    dependents.validate(r);
    retireeSupport.validate(r);
  }
};

inline constexpr Households kDefaultHouseholds{};
inline constexpr Dependents kDefaultDependents{};
inline constexpr RetireeSupport kDefaultRetireeSupport{};
inline constexpr GraphConfig kDefaultGraphConfig{};

} // namespace PhantomLedger::transfers::family
