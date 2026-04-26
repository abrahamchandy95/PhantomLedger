#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::obligations {

struct Population {
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;
};

[[nodiscard]] std::vector<transactions::Transaction>
scheduledPayments(const entity::product::PortfolioRegistry &registry,
                  time::TimePoint start, time::TimePoint endExcl,
                  const Population &population, random::Rng &rng,
                  const transactions::Factory &txf);

} // namespace PhantomLedger::transfers::obligations
