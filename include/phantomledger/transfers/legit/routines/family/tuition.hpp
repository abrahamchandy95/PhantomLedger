#pragma once

#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/runtime.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::tuition {

struct TuitionSchedule {
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

inline constexpr TuitionSchedule kDefaultTuitionSchedule{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const Runtime &rt, const TuitionSchedule &cfg);

} // namespace PhantomLedger::transfers::legit::routines::family::tuition
