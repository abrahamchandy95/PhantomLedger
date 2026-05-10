#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::insurance {

/// Population view shared across both premium and claim emitters.
struct Population {
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;
};

/// Emits monthly premium debits for every insured person

class PremiumGenerator {
public:
  PremiumGenerator(random::Rng &rng, const transactions::Factory &txf) noexcept
      : rng_(&rng), txf_(&txf) {}

  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const time::Window &window,
           const entity::product::PortfolioRegistry &portfolios,
           const Population &population) const;

private:
  random::Rng *rng_;
  const transactions::Factory *txf_;
};

} // namespace PhantomLedger::transfers::insurance
