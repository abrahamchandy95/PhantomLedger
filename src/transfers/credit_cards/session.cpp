#include "phantomledger/transfers/credit_cards/detail/session.hpp"

#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/credit_cards/cycle.hpp"
#include "phantomledger/transfers/credit_cards/dispute.hpp"
#include "phantomledger/transfers/credit_cards/payment.hpp"
#include "phantomledger/transfers/credit_cards/statement.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace PhantomLedger::transfers::credit_cards::detail {
namespace {

inline constexpr double kAmountSlack = 1e-6;

inline constexpr double kStatementClosedThreshold = 0.01;

inline constexpr time::Minutes kInterestPostOffset{15};

inline constexpr time::Hours kLateFeeMorningHour{10};

inline constexpr time::Hours kDueDateHour{17};

inline constexpr int kDaysPerYear = 365;

} // namespace

Session::Session(const Environment &env, Account account, Purchases purchases,
                 random::Rng rng, std::vector<transactions::Transaction> &out)
    : env_(env), account_(account), purchases_(purchases), rng_(std::move(rng)),
      out_(out) {}

void Session::run(Cycle cycle) {
  events_.clear();
  events_.reserve(64);

  collectPurchases(cycle);
  drainDueCredits(cycle);
  sortEventsByTime();

  const auto snapshot = integrateBalance(account_.card, state_.balance, events_,
                                         cycle.start, cycle.endExcl);
  state_.balance = snapshot.ending;

  accrueInterest(snapshot.average, cycle);

  const double statementAbs = std::max(0.0, -state_.balance);
  if (statementAbs <= kStatementClosedThreshold) {
    state_.inGrace = true;
    return;
  }

  const double minimumDueAmt =
      primitives::utils::roundMoney(minimumDue(env_.billing, statementAbs));
  const time::TimePoint due =
      cycle.endExcl + time::Days{env_.billing.graceDays} + kDueDateHour;

  const PaymentIntent intent = draftPayment(statementAbs, minimumDueAmt, due);

  const bool paidOnTime = (intent.amount >= minimumDueAmt - kAmountSlack) &&
                          isOnTime(intent.when, due);
  const bool paidFullOnTime = (intent.amount >= statementAbs - kAmountSlack) &&
                              isOnTime(intent.when, due);

  postPayment(intent, cycle.windowEndExcl);

  if (!paidOnTime && env_.billing.lateFee > 0.0) {
    postLateFee(due, cycle.windowEndExcl);
  }

  state_.inGrace = paidFullOnTime;
}

// ----- Phase 1: pull cycle's purchases and dispute draws ---------------

void Session::collectPurchases(Cycle cycle) {
  const std::int64_t cycleEndEpoch = time::toEpochSeconds(cycle.endExcl);

  while (state_.purchaseCursor < purchases_.indices.size()) {
    const auto txnIx = purchases_.indices[state_.purchaseCursor];
    const auto &purchase = purchases_.txns[txnIx];
    if (purchase.timestamp >= cycleEndEpoch) {
      break;
    }

    events_.push_back(purchase);

    auto credit =
        sampleMerchantCredit(env_.disputes.window, env_.disputes.rates, rng_,
                             purchase, cycle.windowEndExcl, env_.factory);
    if (credit.has_value()) {
      state_.deferredCredits.push_back(*credit);
      out_.push_back(std::move(*credit));
    }
    ++state_.purchaseCursor;
  }
}

// ----- Phase 2: drain merchant credits whose timestamps land here ------

void Session::drainDueCredits(Cycle cycle) {
  const std::int64_t startEpoch = time::toEpochSeconds(cycle.start);
  const std::int64_t endEpoch = time::toEpochSeconds(cycle.endExcl);

  auto &deferred = state_.deferredCredits;
  auto partition = std::partition(deferred.begin(), deferred.end(),
                                  [&](const transactions::Transaction &c) {
                                    return c.timestamp < startEpoch ||
                                           c.timestamp >= endEpoch;
                                  });

  for (auto it = partition; it != deferred.end(); ++it) {
    events_.push_back(std::move(*it));
  }
  deferred.erase(partition, deferred.end());
}

void Session::sortEventsByTime() {
  std::sort(events_.begin(), events_.end(),
            [](const transactions::Transaction &a,
               const transactions::Transaction &b) {
              return a.timestamp < b.timestamp;
            });
}

// ----- Phase 3: interest accrual ---------------------------------------

void Session::accrueInterest(double averageBalance, Cycle cycle) {
  if (state_.inGrace) {
    return;
  }
  const double interval = intervalDays(cycle.start, cycle.endExcl);
  const double debtAvg = std::max(0.0, -averageBalance);
  const double rawInterest = debtAvg * (account_.apr / kDaysPerYear) * interval;
  const auto interest = billableInterest(rawInterest);
  if (!interest.has_value()) {
    return;
  }

  book(
      transactions::Draft{
          .source = account_.card,
          .destination = env_.issuerAccount,
          .amount = *interest,
          .timestamp =
              time::toEpochSeconds(cycle.endExcl + kInterestPostOffset),
          .channel = channels::tag(channels::Credit::interest),
      },
      -*interest);
}

// ----- Phase 4: draft a payment based on autopay mode ------------------

Session::PaymentIntent Session::draftPayment(double statementAbs,
                                             double minimumDueAmt,
                                             time::TimePoint due) {
  PaymentIntent intent{};
  switch (account_.autopay) {
  case entity::card::Autopay::full:
    intent.amount = statementAbs;
    intent.when = samplePaymentTime(env_.payments.timing, rng_, due, true);
    break;
  case entity::card::Autopay::minimum:
    intent.amount = minimumDueAmt;
    intent.when = samplePaymentTime(env_.payments.timing, rng_, due, true);
    break;
  case entity::card::Autopay::manual:
    intent.amount = samplePaymentAmount(env_.payments.mixture, rng_,
                                        statementAbs, minimumDueAmt);
    intent.when = samplePaymentTime(env_.payments.timing, rng_, due, false);
    break;
  }
  intent.amount = primitives::utils::roundMoney(std::max(0.0, intent.amount));
  return intent;
}

// ----- Phase 5: post the payment if it falls inside the window ---------

void Session::postPayment(const PaymentIntent &intent,
                          time::TimePoint windowEndExcl) {
  if (intent.amount <= 0.0 || intent.when >= windowEndExcl) {
    return;
  }
  book(
      transactions::Draft{
          .source = account_.funding,
          .destination = account_.card,
          .amount = intent.amount,
          .timestamp = time::toEpochSeconds(intent.when),
          .channel = channels::tag(channels::Credit::payment),
      },
      +intent.amount);
}

// ----- Phase 6: late fee on missed minimum -----------------------------

void Session::postLateFee(time::TimePoint due, time::TimePoint windowEndExcl) {
  const time::TimePoint fallback =
      resolveDueDate(due) + time::Days{1} + kLateFeeMorningHour;
  const time::TimePoint cap = windowEndExcl - time::Seconds{1};
  const time::TimePoint feeTs = std::min(fallback, cap);
  const double fee = primitives::utils::roundMoney(env_.billing.lateFee);

  book(
      transactions::Draft{
          .source = account_.card,
          .destination = env_.issuerAccount,
          .amount = fee,
          .timestamp = time::toEpochSeconds(feeTs),
          .channel = channels::tag(channels::Credit::lateFee),
      },
      -fee);
}

// ----- Primitive: materialize a transaction and book the balance ------

void Session::book(const transactions::Draft &draft, double balanceDelta) {
  out_.push_back(env_.factory.make(draft));
  state_.balance += balanceDelta;
}

} // namespace PhantomLedger::transfers::credit_cards::detail
