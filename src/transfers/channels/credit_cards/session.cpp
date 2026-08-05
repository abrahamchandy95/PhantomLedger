#include "phantomledger/transfers/channels/credit_cards/detail/session.hpp"

#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/channels/credit_cards/cycle.hpp"
#include "phantomledger/transfers/channels/credit_cards/dispute/sampler.hpp"
#include "phantomledger/transfers/channels/credit_cards/payment.hpp"
#include "phantomledger/transfers/channels/credit_cards/statement.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <utility>

namespace PhantomLedger::transfers::credit_cards::detail {
namespace {

inline constexpr double kAmountSlack = 1e-6;
inline constexpr double kStatementClosedThreshold = 0.01;
inline constexpr time::Minutes kInterestPostOffset{15};
inline constexpr time::TimeOfDay kLateFeeMorningTime{10, 0, 0};
inline constexpr time::TimeOfDay kDueDateTime{17, 0, 0};
inline constexpr int kDaysPerYear = 365;

} // namespace

Session::Session(const Environment &env, Account account, random::Rng rng,
                 std::vector<transactions::Transaction> &out)
    : env_(env), account_(account), rng_(std::move(rng)), out_(out) {}

Session::Session(const Environment &env, Account account, random::Rng rng,
                 std::vector<transactions::Transaction> &out,
                 LedgerBinding ledger)
    : env_(env), account_(account), rng_(std::move(rng)), out_(out),
      ledger_(ledger) {}

void Session::run(CardPurchases purchases, Cycle cycle) {
  events_.clear();
  events_.reserve(64);

  collectPurchases(purchases, cycle);
  drainDueBalanceEvents(cycle);
  sortEventsByTime();

  const auto snapshot = integrateBalance(
      account_.card, state_.balance, events_,
      time::HalfOpenInterval{.start = cycle.start, .endExcl = cycle.endExcl});
  state_.balance = snapshot.ending;

  accrueInterest(snapshot.average, cycle);

  const double statementAbs = std::max(0.0, -state_.balance);
  if (statementAbs <= kStatementClosedThreshold) {
    state_.inGrace = true;
    return;
  }

  // H1 step 2b (class P): the $25 minimum-payment floor and the $32
  // late fee are behavioral dollar constants realized at the cycle
  // date's CPI level (authority U-6); percent terms are scale-free
  // and the statement itself aggregates already-scaled purchases.
  BillingTerms billing = env_.billing;
  const double cycleScale = ::PhantomLedger::synth::econ::priceScale(
      time::toCalendarDate(cycle.endExcl).year);
  billing.minPaymentDollars *= cycleScale;
  billing.lateFee *= cycleScale;

  const double minimumDueAmt =
      primitives::utils::roundMoney(minimumDue(billing, statementAbs));
  const time::TimePoint due = time::makeTime(
      time::toCalendarDate(cycle.endExcl + time::Days{billing.graceDays}),
      kDueDateTime);

  const PaymentIntent intent = draftPayment(statementAbs, minimumDueAmt, due);

  const bool paidOnTime = (intent.amount >= minimumDueAmt - kAmountSlack) &&
                          isOnTime(intent.when, due);
  const bool paidFullOnTime = (intent.amount >= statementAbs - kAmountSlack) &&
                              isOnTime(intent.when, due);

  postPayment(intent, due, cycle.windowEndExcl,
              paidOnTime ? billing.lateFee : 0.0);

  if (!paidOnTime && billing.lateFee > 0.0) {
    postLateFee(due, cycle.windowEndExcl, billing.lateFee);
  }

  state_.inGrace = paidFullOnTime;
}

void Session::collectPurchases(CardPurchases purchases, Cycle cycle) {
  const std::int64_t cycleEndEpoch = time::toEpochSeconds(cycle.endExcl);
  const DisputeSampler sampler{env_.disputes, env_.factory};

  while (state_.purchaseCursor < purchases.indices.size()) {
    const auto txnIx = purchases.indices[state_.purchaseCursor];
    const auto &purchase = purchases.txns[txnIx];
    if (purchase.timestamp >= cycleEndEpoch) {
      break;
    }

    events_.push_back(purchase);

    auto credit = sampler.sample(rng_, purchase, cycle.windowEndExcl);
    if (credit.has_value()) {
      const auto creditTxn = std::move(*credit);

      if (ledger_.ledger == nullptr) {
        out_.push_back(creditTxn);
        state_.deferredBalanceEvents.push_back(creditTxn);
      } else if (creditTxn.timestamp < cycleEndEpoch) {
        // The sampler runs at statement close, so an ordinary short-lag
        // refund may already be in this cycle. Resolve its ledger decision
        // now and include it in the statement only when it really posted.
        if (postToLedger(creditTxn)) {
          out_.push_back(creditTxn);
          state_.deferredBalanceEvents.push_back(creditTxn);
        }
      } else {
        pendingLedgerPostings_.push_back(
            PendingLedgerPosting{.txn = creditTxn});
      }
    }
    ++state_.purchaseCursor;
  }
}

void Session::drainDueBalanceEvents(Cycle cycle) {
  const std::int64_t startEpoch = time::toEpochSeconds(cycle.start);
  const std::int64_t endEpoch = time::toEpochSeconds(cycle.endExcl);

  auto &deferred = state_.deferredBalanceEvents;
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

  book(transactions::Draft{
      .source = account_.card,
      .destination = env_.issuerAccount,
      .amount = *interest,
      .timestamp = time::toEpochSeconds(cycle.endExcl + kInterestPostOffset),
      .channel = channels::tag(channels::Credit::interest),
  });
}

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

void Session::postPayment(const PaymentIntent &intent, time::TimePoint due,
                          time::TimePoint windowEndExcl,
                          double lateFeeOnReject) {
  if (intent.amount <= 0.0 || intent.when >= windowEndExcl) {
    return;
  }
  const std::optional<LateFeeOnReject> rejectionConsequence =
      lateFeeOnReject > 0.0 ? std::optional<LateFeeOnReject>{LateFeeOnReject{
                                  .due = due,
                                  .windowEndExcl = windowEndExcl,
                                  .amount = lateFeeOnReject,
                              }}
                            : std::nullopt;
  book(
      transactions::Draft{
          .source = account_.funding,
          .destination = account_.card,
          .amount = intent.amount,
          .timestamp = time::toEpochSeconds(intent.when),
          .channel = channels::tag(channels::Credit::payment),
      },
      rejectionConsequence);
}

void Session::postLateFee(time::TimePoint due, time::TimePoint windowEndExcl,
                          double fee) {
  const time::TimePoint dayAfterDue = resolveDueDate(due) + time::Days{1};
  const time::TimePoint fallback =
      time::makeTime(time::toCalendarDate(dayAfterDue), kLateFeeMorningTime);
  if (fallback >= windowEndExcl) {
    return;
  }
  const double roundedFee = primitives::utils::roundMoney(fee);

  book(transactions::Draft{
      .source = account_.card,
      .destination = env_.issuerAccount,
      .amount = roundedFee,
      .timestamp = time::toEpochSeconds(fallback),
      .channel = channels::tag(channels::Credit::lateFee),
  });
}

void Session::book(const transactions::Draft &draft,
                   std::optional<LateFeeOnReject> lateFeeOnReject) {
  const auto txn = env_.factory.make(draft);
  if (ledger_.ledger == nullptr) {
    // The retained Lifecycle has no screening ledger. Preserve its declared
    // assumption that generated lifecycle rows post successfully, while the
    // production CardCycleDriver waits for the real transfer decision.
    out_.push_back(txn);
    state_.deferredBalanceEvents.push_back(txn);
  } else {
    pendingLedgerPostings_.push_back(PendingLedgerPosting{
        .txn = txn,
        .lateFeeOnReject = lateFeeOnReject,
    });
  }
}

void Session::advanceLedgerTo(time::TimePoint boundExcl) {
  const auto boundEpoch = time::toEpochSeconds(boundExcl);

  // A rejected on-time payment can enqueue a late fee that is itself behind
  // a coarse caller-provided bound, so keep draining until no matured rows
  // remain.
  while (!pendingLedgerPostings_.empty()) {
    std::sort(
        pendingLedgerPostings_.begin(), pendingLedgerPostings_.end(),
        [](const PendingLedgerPosting &lhs, const PendingLedgerPosting &rhs) {
          return transactions::Comparator{
              transactions::Comparator::Scope::fundsTransfer}(lhs.txn, rhs.txn);
        });

    const auto split = std::lower_bound(
        pendingLedgerPostings_.begin(), pendingLedgerPostings_.end(),
        boundEpoch,
        [](const PendingLedgerPosting &posting, std::int64_t bound) {
          return posting.txn.timestamp < bound;
        });
    if (split == pendingLedgerPostings_.begin()) {
      break;
    }

    std::vector<PendingLedgerPosting> matured;
    matured.reserve(
        static_cast<std::size_t>(split - pendingLedgerPostings_.begin()));
    matured.insert(matured.end(),
                   std::make_move_iterator(pendingLedgerPostings_.begin()),
                   std::make_move_iterator(split));
    pendingLedgerPostings_.erase(pendingLedgerPostings_.begin(), split);

    for (const auto &posting : matured) {
      if (postToLedger(posting.txn)) {
        out_.push_back(posting.txn);
        state_.deferredBalanceEvents.push_back(posting.txn);
      } else if (posting.txn.session.channel ==
                 channels::tag(channels::Credit::payment)) {
        // run() can optimistically mark a sampled full/on-time intent as
        // restoring grace. A rejected funding transfer is not a payment.
        state_.inGrace = false;
        if (posting.lateFeeOnReject.has_value()) {
          const auto &fee = *posting.lateFeeOnReject;
          postLateFee(fee.due, fee.windowEndExcl, fee.amount);
        }
      }
    }
  }
}

bool Session::postToLedger(const transactions::Transaction &txn) {
  if (ledger_.ledger == nullptr) {
    return true;
  }

  const auto channel = txn.session.channel;
  bool externalSourceAllowed = false;
  ::PhantomLedger::clearing::Ledger::Index srcIdx =
      ::PhantomLedger::clearing::Ledger::invalid;
  ::PhantomLedger::clearing::Ledger::Index dstIdx =
      ::PhantomLedger::clearing::Ledger::invalid;

  if (channel == channels::tag(channels::Credit::payment)) {
    srcIdx = ledger_.fundingIdx;
    dstIdx = ledger_.cardIdx;
  } else if (channel == channels::tag(channels::Credit::interest) ||
             channel == channels::tag(channels::Credit::lateFee)) {
    srcIdx = ledger_.cardIdx;
    dstIdx = ledger_.issuerIdx;
  } else if (channel == channels::tag(channels::Credit::refund) ||
             channel == channels::tag(channels::Credit::chargeback)) {
    srcIdx = ledger_.ledger->findAccount(txn.source);
    dstIdx = ledger_.cardIdx;
    externalSourceAllowed = txn.source.bank == entity::Bank::external;
  } else {
    return false;
  }

  const bool sourceInvalid =
      srcIdx == ::PhantomLedger::clearing::Ledger::invalid;
  if ((sourceInvalid && !externalSourceAllowed) ||
      dstIdx == ::PhantomLedger::clearing::Ledger::invalid) {
    return false;
  }

  return ledger_.ledger->transfer(srcIdx, dstIdx, txn.amount, channel)
      .accepted();
}

} // namespace PhantomLedger::transfers::credit_cards::detail
