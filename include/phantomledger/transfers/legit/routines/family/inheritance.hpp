#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::inheritance {

// H3 (macro-history-v1, contract docs/h3_mortality_estate.md): estates
// are DEATH-CAUSED. The old uncaused hazard (0.15% of retirees per
// window sweep) is RETIRED — every in-window death now produces one
// NFDA-anchored funeral payment, and an estate distribution to the
// heirs ~30-90 days later (probate settle, declared). Estate SIZE
// keeps the interim lognormal (an SCF-anchored re-derivation is the
// registered upgrade); the funeral anchors to the NFDA 2019 General
// Price List at the calibration year: median funeral with viewing and
// burial $7,640, cremation with viewing $5,150, blended at the ~55%
// 2019 cremation rate -> ~$6,300 (MEASUREMENT blend, CHOICE sigma).
struct InheritanceEvent {
  bool enabled = true;

  // Estate size (interim; SCF re-derivation registered).
  double median = 25000.0;
  double sigma = 1.0;

  // Funeral (NFDA 2019 GPL blend, calibration dollars; realizes at
  // the death year's price level).
  double funeralMedian = 6300.0;
  double funeralSigma = 0.40;
  double funeralFloor = 1000.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::gt("median", median, 0.0); });
    r.check([&] { v::nonNegative("sigma", sigma); });
    r.check([&] { v::gt("funeralMedian", funeralMedian, 0.0); });
    r.check([&] { v::nonNegative("funeralSigma", funeralSigma); });
    r.check([&] { v::nonNegative("funeralFloor", funeralFloor); });
  }
};

inline constexpr InheritanceEvent kDefaultInheritanceEvent{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(const TransferRun &run, const InheritanceEvent &model);

} // namespace PhantomLedger::transfers::legit::routines::family::inheritance
