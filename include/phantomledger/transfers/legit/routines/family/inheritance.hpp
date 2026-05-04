#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::inheritance {

struct InheritanceEvent {
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

inline constexpr InheritanceEvent kDefaultInheritanceEvent{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const TransferRun &run, const InheritanceEvent &model);

} // namespace PhantomLedger::transfers::legit::routines::family::inheritance
