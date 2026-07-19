#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/primitives/random/rng.hpp"
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

class OpeningBook {
public:
  struct Accounts {
    const entity::account::Registry *registry = nullptr;
    const entity::account::Lookup *lookup = nullptr;
    const entity::account::Ownership *ownership = nullptr;
  };

  struct Protections {
    const clearing::BalanceRules *balanceRules = nullptr;
    const entity::product::PortfolioRegistry *portfolios = nullptr;
    const entity::card::Registry *creditCards = nullptr;
  };

  OpeningBook() = default;
  OpeningBook(random::Rng &rng, Accounts accounts,
              Protections protections) noexcept;

  OpeningBook &rng(random::Rng &value) noexcept;
  OpeningBook &accounts(Accounts value) noexcept;
  OpeningBook &protections(Protections value) noexcept;

  [[nodiscard]] std::unique_ptr<clearing::Ledger>
  build(const blueprints::LegitBlueprint &plan) const;

private:
  random::Rng *rng_ = nullptr;
  Accounts accounts_{};
  Protections protections_{};
};

} // namespace PhantomLedger::transfers::legit::ledger
