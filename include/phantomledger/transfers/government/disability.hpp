#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/government/recipients.hpp"

#include <vector>

namespace PhantomLedger::transfers::government {

/// Knobs for the disability subsystem only.
struct DisabilityTerms {
  double eligibleP = 0.04;
  double median = 1630.0;
  double sigma = 0.25;
  double floor = 500.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("eligibleP", eligibleP); });
    r.check([&] { v::positive("median", median); });
    r.check([&] { v::nonNegative("sigma", sigma); });
    r.check([&] { v::nonNegative("floor", floor); });
  }
};

/// Emit disability benefit deposits inside the window. Sorted by
/// timestamp on return.
[[nodiscard]] std::vector<transactions::Transaction>
disabilityBenefits(const DisabilityTerms &terms, const time::Window &window,
                   random::Rng &rng, const transactions::Factory &txf,
                   const Population &population,
                   const entity::Key &disabilityCounterparty);

} // namespace PhantomLedger::transfers::government
