#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/insurance_ledger.hpp"
#include "phantomledger/entities/products/loan_terms_ledger.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::insurance {

struct Population {
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;
};

class PremiumGenerator {
public:
  PremiumGenerator(random::Rng &rng, const transactions::Factory &txf) noexcept
      : rng_(&rng), txf_(&txf) {}

  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const time::Window &window,
           const entity::product::InsuranceLedger &insurance,
           const entity::product::LoanTermsLedger &loans,
           const Population &population) const;

private:
  random::Rng *rng_;
  const transactions::Factory *txf_;
};

} // namespace PhantomLedger::transfers::insurance
