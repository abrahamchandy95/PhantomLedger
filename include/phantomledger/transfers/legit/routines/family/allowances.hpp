#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::allowances {

struct AllowanceSchedule {
  bool enabled = true;
  double weeklyP = 0.70;
  double paretoXm = 35.0;
  double paretoAlpha = 2.2;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("weeklyP", weeklyP, 0.0, 1.0); });
    r.check([&] { v::gt("paretoXm", paretoXm, 0.0); });
    r.check([&] { v::gt("paretoAlpha", paretoAlpha, 0.0); });
  }
};

inline constexpr AllowanceSchedule kDefaultRules{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const TransferRun &run, const AllowanceSchedule &cfg);

} // namespace PhantomLedger::transfers::legit::routines::family::allowances
