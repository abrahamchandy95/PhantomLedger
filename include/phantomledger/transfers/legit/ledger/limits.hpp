#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <memory>

namespace PhantomLedger::clearing {
struct BalanceRules;
} // namespace PhantomLedger::clearing

namespace PhantomLedger::entity::product {
class PortfolioRegistry;
} // namespace PhantomLedger::entity::product

namespace PhantomLedger::transfers::legit::ledger {

struct BalanceBookRequest {
  time::Window window{};
  random::Rng *rng = nullptr;

  const entity::account::Registry *accounts = nullptr;
  const entity::account::Lookup *accountsLookup = nullptr;
  const entity::account::Ownership *ownership = nullptr;

  const clearing::BalanceRules *rules = nullptr;
  const entity::product::PortfolioRegistry *portfolios = nullptr;
  const entity::card::Registry *creditCards = nullptr;
};

[[nodiscard]] std::unique_ptr<clearing::Ledger>
buildBalanceBook(const BalanceBookRequest &request,
                 const blueprints::LegitBuildPlan &plan);

} // namespace PhantomLedger::transfers::legit::ledger
