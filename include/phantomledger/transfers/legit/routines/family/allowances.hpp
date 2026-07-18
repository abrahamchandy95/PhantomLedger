#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::allowances {

struct AllowanceSchedule {
  bool enabled = true;
  double weeklyP = 0.70;
  // Pareto(xm $8, alpha 1.8): mean ~$18/wk against platform
  // transaction data averaging $13-17/wk, median ~$10/wk — the old
  // Pareto($35, 2.2) had a FLOOR above every measured average
  // (household-econ-2026-07; docs/fraud_model_audit.md L-9).
  double paretoXm = 8.0;
  double paretoAlpha = 1.8;

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
