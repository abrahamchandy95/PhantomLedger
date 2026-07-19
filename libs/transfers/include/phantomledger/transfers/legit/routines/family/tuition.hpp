#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::tuition {

struct TuitionSchedule {
  bool enabled = true;
  double p = 0.65;
  int instMin = 4;
  int instMax = 5;
  double mu = 8.95;
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
generate(const TransferRun &run, const TuitionSchedule &cfg);

} // namespace PhantomLedger::transfers::legit::routines::family::tuition
