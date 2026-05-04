#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::parent_gifts {

struct ParentGiftFlow {
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

inline constexpr ParentGiftFlow kDefaultParentGiftFlow{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const TransferRun &run, const ParentGiftFlow &model);

} // namespace PhantomLedger::transfers::legit::routines::family::parent_gifts
