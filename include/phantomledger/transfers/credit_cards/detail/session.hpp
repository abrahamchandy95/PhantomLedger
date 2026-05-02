#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/credit_cards/dispute.hpp"
#include "phantomledger/transfers/credit_cards/payment.hpp"
#include "phantomledger/transfers/credit_cards/statement.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::credit_cards::detail {

/// Immutable per-card terms resolved once per session.
struct Account {
  entity::Key card;
  entity::Key funding;
  double apr = 0.0;
  std::uint8_t cycleDay = 1;
  entity::card::Autopay autopay = entity::card::Autopay::manual;
};

/// Mutable session state carried across cycles.
struct State {
  double balance = 0.0;
  bool inGrace = true;
  std::size_t purchaseCursor = 0;
  std::vector<transactions::Transaction> deferredCredits;
};

struct Cycle {
  time::TimePoint start;
  time::TimePoint endExcl;
  time::TimePoint windowEndExcl;
};

struct Purchases {
  std::span<const transactions::Transaction> txns;
  std::span<const std::uint32_t> indices;
};

struct Environment {
  const BillingTerms &billing;
  const PaymentBehavior &payments;
  const DisputeBehavior &disputes;
  const transactions::Factory &factory;
  entity::Key issuerAccount;
};

class Session {
public:
  Session(const Environment &env, Account account, Purchases purchases,
          random::Rng rng, std::vector<transactions::Transaction> &out);

  void run(Cycle cycle);

private:
  struct PaymentIntent {
    double amount;
    time::TimePoint when;
  };

  void collectPurchases(Cycle cycle);
  void drainDueCredits(Cycle cycle);
  void sortEventsByTime();
  void accrueInterest(double averageBalance, Cycle cycle);
  [[nodiscard]] PaymentIntent
  draftPayment(double statementAbs, double minimumDueAmt, time::TimePoint due);
  void postPayment(const PaymentIntent &intent, time::TimePoint windowEndExcl);
  void postLateFee(time::TimePoint due, time::TimePoint windowEndExcl);

  void book(const transactions::Draft &draft, double balanceDelta);

  const Environment &env_;
  Account account_;
  Purchases purchases_;
  State state_{};
  random::Rng rng_;
  std::vector<transactions::Transaction> &out_;

  std::vector<transactions::Transaction> events_{};
};

} // namespace PhantomLedger::transfers::credit_cards::detail
