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

[[nodiscard]] std::vector<transactions::Transaction>
premiums(const time::Window &window, random::Rng &rng,
         const transactions::Factory &txf,
         const entity::product::PortfolioRegistry &portfolios,
         const Population &population);

} // namespace PhantomLedger::transfers::insurance
