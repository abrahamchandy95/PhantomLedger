#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/dispute/behavior.hpp"
#include "phantomledger/transfers/channels/credit_cards/payment.hpp"
#include "phantomledger/transfers/channels/credit_cards/statement.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::credit_cards::detail {

struct Account {
  entity::Key card;
  entity::Key funding;
  double apr = 0.0;
  std::uint8_t cycleDay = 1;
  entity::card::Autopay autopay = entity::card::Autopay::manual;
};

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

struct CardPurchases {
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

struct LedgerBinding {
  ::PhantomLedger::clearing::Ledger *ledger = nullptr;
  ::PhantomLedger::clearing::Ledger::Index cardIdx =
      ::PhantomLedger::clearing::Ledger::invalid;
  ::PhantomLedger::clearing::Ledger::Index fundingIdx =
      ::PhantomLedger::clearing::Ledger::invalid;
  ::PhantomLedger::clearing::Ledger::Index issuerIdx =
      ::PhantomLedger::clearing::Ledger::invalid;
};

class Session {
public:
  Session(const Environment &env, Account account, random::Rng rng,
          std::vector<transactions::Transaction> &out);

  Session(const Environment &env, Account account, random::Rng rng,
          std::vector<transactions::Transaction> &out, LedgerBinding ledger);

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) noexcept = default;
  Session &operator=(Session &&) noexcept = delete;

  void run(CardPurchases purchases, Cycle cycle);

  [[nodiscard]] const Account &account() const noexcept { return account_; }

private:
  struct PaymentIntent {
    double amount;
    time::TimePoint when;
  };

  void collectPurchases(CardPurchases purchases, Cycle cycle);
  void drainDueCredits(Cycle cycle);
  void sortEventsByTime();
  void accrueInterest(double averageBalance, Cycle cycle);
  [[nodiscard]] PaymentIntent
  draftPayment(double statementAbs, double minimumDueAmt, time::TimePoint due);
  void postPayment(const PaymentIntent &intent, time::TimePoint windowEndExcl);
  void postLateFee(time::TimePoint due, time::TimePoint windowEndExcl);

  void book(const transactions::Draft &draft, double balanceDelta);
  void postToLedger(const transactions::Draft &draft);

  const Environment &env_;
  Account account_;
  State state_{};
  random::Rng rng_;
  std::vector<transactions::Transaction> &out_;
  LedgerBinding ledger_{};

  std::vector<transactions::Transaction> events_{};
};

} // namespace PhantomLedger::transfers::credit_cards::detail
