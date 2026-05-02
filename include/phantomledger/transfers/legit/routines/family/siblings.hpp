#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/runtime.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::siblings {

struct SiblingFlow {
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

inline constexpr SiblingFlow kDefaultSiblingFlow{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const Runtime &rt, const SiblingFlow &model);

} // namespace PhantomLedger::transfers::legit::routines::family::siblings
