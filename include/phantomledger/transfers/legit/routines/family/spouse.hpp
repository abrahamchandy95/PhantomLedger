#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::spouse {

struct CoupleFlow {
  bool enabled = true;
  double separateAccountsP = 0.60;
  int txnsPerMonthMin = 2;
  int txnsPerMonthMax = 6;
  double breadwinnerFlowP = 0.65;
  double transferMedian = 85.0;
  double transferSigma = 0.9;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check(
        [&] { v::between("separateAccountsP", separateAccountsP, 0.0, 1.0); });
    r.check([&] { v::ge("txnsPerMonthMin", txnsPerMonthMin, 1); });
    r.check(
        [&] { v::ge("txnsPerMonthMax", txnsPerMonthMax, txnsPerMonthMin); });
    r.check(
        [&] { v::between("breadwinnerFlowP", breadwinnerFlowP, 0.0, 1.0); });
    r.check([&] { v::gt("transferMedian", transferMedian, 0.0); });
    r.check([&] { v::nonNegative("transferSigma", transferSigma); });
  }
};

inline constexpr CoupleFlow kDefaultCoupleFlow{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const TransferRun &run, const CoupleFlow &model);

} // namespace PhantomLedger::transfers::legit::routines::family::spouse
