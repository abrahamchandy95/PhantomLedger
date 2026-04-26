#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/credit_cards/policy/behavior.hpp"
#include "phantomledger/transfers/credit_cards/policy/issuer.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::credit_cards {

struct LedgerView {
  const entity::card::Registry &cards;
  const std::unordered_map<entity::PersonId, entity::Key> &primaryAccounts;
  entity::Key issuerAccount;
};

/// Generates credit-card lifecycle transactions for a window.
class Lifecycle {
public:
  Lifecycle(const IssuerPolicy &policy, const CardholderBehavior &behavior,
            const transactions::Factory &factory,
            const random::RngFactory &rngFactory, LedgerView ledger);

  /// Generate lifecycle transactions for the window.
  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const time::Window &window,
           std::span<const transactions::Transaction> txns) const;

private:
  const IssuerPolicy &policy_;
  const CardholderBehavior &behavior_;
  const transactions::Factory &factory_;
  const random::RngFactory &rngFactory_;
  LedgerView ledger_;
};

} // namespace PhantomLedger::transfers::credit_cards
