#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/runtime.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::support {

struct SupportFlow {
  bool enabled = true;
  double supportP = 0.35;
  double paretoXm = 25.0;
  double paretoAlpha = 2.4;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("supportP", supportP, 0.0, 1.0); });
    r.check([&] { v::gt("paretoXm", paretoXm, 0.0); });
    r.check([&] { v::gt("paretoAlpha", paretoAlpha, 0.0); });
  }
};

inline constexpr SupportFlow kDefaultSupportFlow{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const Runtime &rt, const SupportFlow &model);

} // namespace PhantomLedger::transfers::legit::routines::family::support
