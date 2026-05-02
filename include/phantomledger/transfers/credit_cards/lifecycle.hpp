#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/credit_cards/dispute.hpp"
#include "phantomledger/transfers/credit_cards/payment.hpp"
#include "phantomledger/transfers/credit_cards/statement.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::credit_cards {

struct LedgerView {
  const entity::card::Registry &cards;
  const std::unordered_map<entity::PersonId, entity::Key> &primaryAccounts;
  entity::Key issuerAccount;
};

struct LifecycleRules {
  BillingTerms billing{};
  PaymentBehavior payments{};
  DisputeBehavior disputes{};

  void validate(primitives::validate::Report &r) const {
    billing.validate(r);
    payments.validate(r);
    disputes.validate(r);
  }
};

inline constexpr LifecycleRules kDefaultLifecycleRules{};

/// Generates credit-card lifecycle transactions for a window.
class Lifecycle {
public:
  Lifecycle(const LifecycleRules &rules, const transactions::Factory &factory,
            const random::RngFactory &rngFactory, LedgerView ledger);

  /// Generate lifecycle transactions for the window.
  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const time::Window &window,
           std::span<const transactions::Transaction> txns) const;

private:
  const LifecycleRules &rules_;
  const transactions::Factory &factory_;
  const random::RngFactory &rngFactory_;
  LedgerView ledger_;
};

} // namespace PhantomLedger::transfers::credit_cards
