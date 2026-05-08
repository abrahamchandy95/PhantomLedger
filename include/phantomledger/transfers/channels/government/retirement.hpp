#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/government/recipients.hpp"

#include <vector>

namespace PhantomLedger::transfers::government {

struct RetirementTerms {
  double eligibleP = 0.87;
  double median = 2071.0;
  double sigma = 0.30;
  double floor = 900.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("eligibleP", eligibleP); });
    r.check([&] { v::positive("median", median); });
    r.check([&] { v::nonNegative("sigma", sigma); });
    r.check([&] { v::nonNegative("floor", floor); });
  }
};

[[nodiscard]] std::vector<transactions::Transaction>
retirementBenefits(const RetirementTerms &terms, const time::Window &window,
                   random::Rng &rng, const transactions::Factory &txf,
                   const Population &population,
                   const entity::Key &ssaCounterparty);

} // namespace PhantomLedger::transfers::government
