#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/runtime.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::grandparent_gifts {

struct GrandparentGiftFlow {
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

inline constexpr GrandparentGiftFlow kDefaultGrandparentGiftFlow{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const Runtime &rt, const GrandparentGiftFlow &model);

} // namespace
  // PhantomLedger::transfers::legit::routines::family::grandparent_gifts
