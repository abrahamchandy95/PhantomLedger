#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/channels/credit_cards/cycle.hpp"
#include "phantomledger/transfers/channels/credit_cards/detail/session.hpp"
#include "phantomledger/transfers/channels/credit_cards/payment.hpp"

#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

using namespace PhantomLedger;
namespace credit_cards = PhantomLedger::transfers::credit_cards;

namespace {

inline constexpr entity::Key kCard{
    identifiers::Role::card,
    identifiers::Bank::internal,
    1,
};
inline constexpr entity::Key kFunding{
    identifiers::Role::account,
    identifiers::Bank::internal,
    2,
};
inline constexpr entity::Key kIssuer{
    identifiers::Role::processor,
    identifiers::Bank::external,
    3,
};
inline constexpr entity::Key kMerchant{
    identifiers::Role::merchant,
    identifiers::Bank::external,
    4,
};

[[nodiscard]] time::TimePoint at(int year, unsigned month, unsigned day,
                                 int hour = 0, int minute = 0) {
  return time::makeTime({year, month, day}, {hour, minute, 0});
}

[[nodiscard]] const transactions::Transaction *
findChannel(const std::vector<transactions::Transaction> &rows,
            channels::Tag channel) {
  for (const auto &row : rows) {
    if (row.session.channel == channel) {
      return &row;
    }
  }
  return nullptr;
}

[[nodiscard]] std::size_t
countChannel(const std::vector<transactions::Transaction> &rows,
             channels::Tag channel) {
  std::size_t count = 0;
  for (const auto &row : rows) {
    if (row.session.channel == channel) {
      ++count;
    }
  }
  return count;
}

[[nodiscard]] std::vector<transactions::Transaction>
runCardSession(entity::card::Autopay autopay,
               const credit_cards::PaymentBehavior &payments,
               std::uint64_t seed = 17) {
  const credit_cards::BillingTerms billing{};
  credit_cards::DisputeBehavior disputes{};
  disputes.rates.refundProbability = 0.0;
  disputes.rates.chargebackProbability = 0.0;

  auto factoryRng = random::Rng::fromSeed(91);
  const transactions::Factory factory(factoryRng);
  const credit_cards::detail::Environment env{billing, payments, disputes,
                                              factory, kIssuer};

  transactions::Transaction purchase{};
  purchase.source = kCard;
  purchase.target = kMerchant;
  purchase.amount = 100.0;
  purchase.timestamp = time::toEpochSeconds(at(2019, 1, 5, 14, 30));
  purchase.session.channel = channels::tag(channels::Legit::cardPurchase);

  const std::array<transactions::Transaction, 1> purchases{purchase};
  const std::array<std::uint32_t, 1> indices{0};
  std::vector<transactions::Transaction> emitted;

  credit_cards::detail::Session session(env,
                                        credit_cards::detail::Account{
                                            .card = kCard,
                                            .funding = kFunding,
                                            .apr = 0.20,
                                            .cycleDay = 10,
                                            .autopay = autopay,
                                        },
                                        random::Rng::fromSeed(seed), emitted);

  session.run(
      credit_cards::detail::CardPurchases{
          .txns = std::span<const transactions::Transaction>(purchases),
          .indices = std::span<const std::uint32_t>(indices),
      },
      credit_cards::detail::Cycle{
          .start = at(2019, 1, 1),
          .endExcl = at(2019, 1, 10, 23, 30),
          .windowEndExcl = at(2019, 3, 1),
      });

  return emitted;
}

void testAutopayModesAreOnTime() {
  const credit_cards::PaymentBehavior payments{};
  const auto paymentTag = channels::tag(channels::Credit::payment);
  const auto lateFeeTag = channels::tag(channels::Credit::lateFee);
  const auto expectedPaymentTime = time::toEpochSeconds(at(2019, 2, 4, 12, 0));

  const auto full = runCardSession(entity::card::Autopay::full, payments, 101);
  const auto *fullPayment = findChannel(full, paymentTag);
  PL_CHECK(fullPayment != nullptr);
  PL_CHECK_EQ(fullPayment->amount, 100.0);
  PL_CHECK_EQ(fullPayment->timestamp, expectedPaymentTime);
  PL_CHECK_EQ(countChannel(full, lateFeeTag), 0U);

  const auto minimum =
      runCardSession(entity::card::Autopay::minimum, payments, 202);
  const auto *minimumPayment = findChannel(minimum, paymentTag);
  PL_CHECK(minimumPayment != nullptr);
  PL_CHECK_EQ(minimumPayment->amount, 25.0);
  PL_CHECK_EQ(minimumPayment->timestamp, expectedPaymentTime);
  PL_CHECK_EQ(countChannel(minimum, lateFeeTag), 0U);

  std::puts("  PASS: full and minimum autopay post at noon on the due date "
            "without a late fee");
}

void testManualSamplerMatchesLateClassification() {
  const auto weekdayDue = at(2019, 2, 4, 17, 0);
  credit_cards::PaymentTiming onTime{};
  onTime.lateProbability = 0.0;

  for (std::uint64_t seed = 0; seed < 256; ++seed) {
    auto rng = random::Rng::fromSeed(seed);
    const auto sampled =
        credit_cards::samplePaymentTime(onTime, rng, weekdayDue, false);
    PL_CHECK(credit_cards::isOnTime(sampled, weekdayDue));
  }

  const auto weekendDue = at(2019, 2, 9, 17, 0);
  credit_cards::PaymentTiming late{};
  late.lateProbability = 1.0;
  late.lateDaysMin = 1;
  late.lateDaysMax = 1;

  for (std::uint64_t seed = 0; seed < 256; ++seed) {
    auto rng = random::Rng::fromSeed(seed);
    const auto sampled =
        credit_cards::samplePaymentTime(late, rng, weekendDue, false);
    PL_CHECK(!credit_cards::isOnTime(sampled, weekendDue));
  }

  std::puts("  PASS: manual on-time and late branches agree with due-date "
            "classification, including weekend adjustment");
}

void testManualLateFeeBehavior() {
  credit_cards::PaymentBehavior onTime{};
  onTime.mixture.payFull = 1.0;
  onTime.mixture.payPartial = 0.0;
  onTime.mixture.payMin = 0.0;
  onTime.mixture.miss = 0.0;
  onTime.timing.lateProbability = 0.0;

  const auto lateFeeTag = channels::tag(channels::Credit::lateFee);
  const auto paid = runCardSession(entity::card::Autopay::manual, onTime, 303);
  PL_CHECK_EQ(countChannel(paid, lateFeeTag), 0U);

  auto late = onTime;
  late.timing.lateProbability = 1.0;
  late.timing.lateDaysMin = 1;
  late.timing.lateDaysMax = 1;

  const auto overdue = runCardSession(entity::card::Autopay::manual, late, 404);
  const auto *lateFee = findChannel(overdue, lateFeeTag);
  PL_CHECK(lateFee != nullptr);
  PL_CHECK_EQ(lateFee->timestamp, time::toEpochSeconds(at(2019, 2, 5, 10, 0)));

  std::puts("  PASS: manual late fee appears only for a sampled late payment "
            "and posts at 10:00 the next day");
}

void testFuturePostingDoesNotTimeTravel() {
  const credit_cards::BillingTerms billing{};
  const credit_cards::PaymentBehavior payments{};
  credit_cards::DisputeBehavior disputes{};
  disputes.rates.refundProbability = 0.0;
  disputes.rates.chargebackProbability = 0.0;

  auto factoryRng = random::Rng::fromSeed(808);
  const transactions::Factory factory(factoryRng);
  const credit_cards::detail::Environment env{billing, payments, disputes,
                                              factory, kIssuer};

  clearing::Ledger ledger;
  ledger.initialize(3);
  ledger.addAccount(kCard, 0);
  ledger.addAccount(kFunding, 1);
  ledger.addAccount(kIssuer, 2);
  ledger.setOverdraftOnly(0, 1'000.0);
  ledger.cash(1) = 1'000.0;
  ledger.createHub(2);

  transactions::Transaction purchase{};
  purchase.source = kCard;
  purchase.target = kMerchant;
  purchase.amount = 100.0;
  purchase.timestamp = time::toEpochSeconds(at(2019, 1, 5, 14, 30));
  purchase.session.channel = channels::tag(channels::Legit::cardPurchase);

  const std::array<transactions::Transaction, 1> purchases{purchase};
  const std::array<std::uint32_t, 1> indices{0};
  std::vector<transactions::Transaction> emitted;

  credit_cards::detail::Session session(
      env,
      credit_cards::detail::Account{
          .card = kCard,
          .funding = kFunding,
          .apr = 0.20,
          .cycleDay = 10,
          .autopay = entity::card::Autopay::full,
      },
      random::Rng::fromSeed(909), emitted,
      credit_cards::detail::LedgerBinding{
          .ledger = &ledger,
          .cardIdx = 0,
          .fundingIdx = 1,
          .issuerIdx = 2,
      });

  session.run(
      credit_cards::detail::CardPurchases{
          .txns = std::span<const transactions::Transaction>(purchases),
          .indices = std::span<const std::uint32_t>(indices),
      },
      credit_cards::detail::Cycle{
          .start = at(2019, 1, 1),
          .endExcl = at(2019, 1, 10, 23, 30),
          .windowEndExcl = at(2019, 3, 1),
      });

  PL_CHECK(emitted.empty());
  PL_CHECK_EQ(ledger.cash(1), 1'000.0);
  PL_CHECK_EQ(ledger.cash(0), 0.0);

  const auto paymentTime = at(2019, 2, 4, 12, 0);
  session.advanceLedgerTo(paymentTime);
  PL_CHECK(emitted.empty());
  PL_CHECK_EQ(ledger.cash(1), 1'000.0);
  PL_CHECK_EQ(ledger.cash(0), 0.0);

  session.advanceLedgerTo(paymentTime + time::Seconds{1});
  PL_CHECK_EQ(countChannel(emitted, channels::tag(channels::Credit::payment)),
              1U);
  PL_CHECK_EQ(ledger.cash(1), 900.0);
  PL_CHECK_EQ(ledger.cash(0), 100.0);
  session.advanceLedgerTo(at(2019, 3, 1));
  PL_CHECK_EQ(countChannel(emitted, channels::tag(channels::Credit::payment)),
              1U);
  PL_CHECK_EQ(ledger.cash(1), 900.0);
  PL_CHECK_EQ(ledger.cash(0), 100.0);

  std::puts("  PASS: future card payment changes spending liquidity only "
            "after its posting timestamp");
}

void testRejectedPaymentDoesNotReduceStatement() {
  const credit_cards::BillingTerms billing{};
  const credit_cards::PaymentBehavior payments{};
  credit_cards::DisputeBehavior disputes{};
  disputes.rates.refundProbability = 0.0;
  disputes.rates.chargebackProbability = 0.0;

  auto factoryRng = random::Rng::fromSeed(1'414);
  const transactions::Factory factory(factoryRng);
  const credit_cards::detail::Environment env{billing, payments, disputes,
                                              factory, kIssuer};

  clearing::Ledger ledger;
  ledger.initialize(3);
  ledger.addAccount(kCard, 0);
  ledger.addAccount(kFunding, 1);
  ledger.addAccount(kIssuer, 2);
  ledger.setOverdraftOnly(0, 1'000.0);
  ledger.cash(1) = 0.0;
  ledger.createHub(2);

  transactions::Transaction purchase{};
  purchase.source = kCard;
  purchase.target = kMerchant;
  purchase.amount = 100.0;
  purchase.timestamp = time::toEpochSeconds(at(2019, 1, 5, 14, 30));
  purchase.session.channel = channels::tag(channels::Legit::cardPurchase);
  const std::array<transactions::Transaction, 1> purchases{purchase};
  const std::array<std::uint32_t, 1> indices{0};
  const credit_cards::detail::CardPurchases purchaseView{
      .txns = std::span<const transactions::Transaction>(purchases),
      .indices = std::span<const std::uint32_t>(indices),
  };
  std::vector<transactions::Transaction> emitted;

  credit_cards::detail::Session session(
      env,
      credit_cards::detail::Account{
          .card = kCard,
          .funding = kFunding,
          .apr = 0.20,
          .cycleDay = 10,
          .autopay = entity::card::Autopay::full,
      },
      random::Rng::fromSeed(1'515), emitted,
      credit_cards::detail::LedgerBinding{
          .ledger = &ledger,
          .cardIdx = 0,
          .fundingIdx = 1,
          .issuerIdx = 2,
      });

  session.run(purchaseView, {
                                .start = at(2019, 1, 1),
                                .endExcl = at(2019, 1, 10, 23, 30),
                                .windowEndExcl = at(2019, 4, 1),
                            });
  const auto firstPaymentTime = at(2019, 2, 4, 12, 0);
  session.advanceLedgerTo(firstPaymentTime + time::Seconds{1});
  PL_CHECK(emitted.empty());
  PL_CHECK_EQ(ledger.cash(1), 0.0);
  PL_CHECK_EQ(ledger.cash(0), 0.0);

  session.advanceLedgerTo(at(2019, 2, 10, 23, 30));
  const auto *lateFee =
      findChannel(emitted, channels::tag(channels::Credit::lateFee));
  PL_CHECK(lateFee != nullptr);
  const double nextStatementAmount = 100.0 + lateFee->amount;

  session.run(purchaseView, {
                                .start = at(2019, 1, 10, 23, 30),
                                .endExcl = at(2019, 2, 10, 23, 30),
                                .windowEndExcl = at(2019, 4, 1),
                            });

  // Make the second attempt observable: it should be for the full original
  // statement because the first, rejected transfer never posted.
  ledger.cash(1) = 1'000.0;
  session.advanceLedgerTo(at(2019, 3, 7, 12, 0) + time::Seconds{1});

  std::array<double, 1> paymentAmounts{};
  std::size_t paymentCount = 0;
  const auto paymentTag = channels::tag(channels::Credit::payment);
  for (const auto &row : emitted) {
    if (row.session.channel == paymentTag) {
      PL_CHECK(paymentCount < paymentAmounts.size());
      paymentAmounts[paymentCount++] = row.amount;
    }
  }
  PL_CHECK_EQ(paymentCount, 1U);
  PL_CHECK_EQ(paymentAmounts[0], nextStatementAmount);
  PL_CHECK_EQ(countChannel(emitted, channels::tag(channels::Credit::lateFee)),
              1U);
  PL_CHECK(countChannel(emitted, channels::tag(channels::Credit::interest)) >
           0);

  std::puts("  PASS: a funding-rejected payment is absent, loses grace, and "
            "posts its late fee into the next statement");
}

void testAcceptedRefundRestoresLedgerAndStatement() {
  const credit_cards::BillingTerms billing{};
  const credit_cards::PaymentBehavior payments{};
  credit_cards::DisputeBehavior disputes{};
  disputes.rates.refundProbability = 1.0;
  disputes.rates.chargebackProbability = 0.0;
  disputes.window.refundMin = 1;
  disputes.window.refundMax = 1;

  auto factoryRng = random::Rng::fromSeed(1'616);
  const transactions::Factory factory(factoryRng);
  const credit_cards::detail::Environment env{billing, payments, disputes,
                                              factory, kIssuer};

  clearing::Ledger ledger;
  ledger.initialize(3);
  ledger.addAccount(kCard, 0);
  ledger.addAccount(kFunding, 1);
  ledger.addAccount(kIssuer, 2);
  ledger.setOverdraftOnly(0, 1'000.0);
  ledger.cash(1) = 1'000.0;
  ledger.createHub(2);

  transactions::Transaction purchase{};
  purchase.source = kCard;
  purchase.target = kMerchant;
  purchase.amount = 100.0;
  purchase.timestamp = time::toEpochSeconds(at(2019, 1, 5, 14, 30));
  purchase.session.channel = channels::tag(channels::Legit::cardPurchase);
  PL_CHECK(ledger
               .transfer(0, clearing::Ledger::invalid, purchase.amount,
                         channels::tag(channels::Legit::cardPurchase))
               .accepted());
  const double cardCashAfterPurchase = ledger.cash(0);

  const std::array<transactions::Transaction, 1> purchases{purchase};
  const std::array<std::uint32_t, 1> indices{0};
  std::vector<transactions::Transaction> emitted;
  credit_cards::detail::Session session(
      env,
      credit_cards::detail::Account{
          .card = kCard,
          .funding = kFunding,
          .apr = 0.20,
          .cycleDay = 10,
          .autopay = entity::card::Autopay::full,
      },
      random::Rng::fromSeed(1'717), emitted,
      credit_cards::detail::LedgerBinding{
          .ledger = &ledger,
          .cardIdx = 0,
          .fundingIdx = 1,
          .issuerIdx = 2,
      });

  session.run(
      {
          .txns = std::span<const transactions::Transaction>(purchases),
          .indices = std::span<const std::uint32_t>(indices),
      },
      {
          .start = at(2019, 1, 1),
          .endExcl = at(2019, 1, 10, 23, 30),
          .windowEndExcl = at(2019, 3, 1),
      });

  PL_CHECK_EQ(countChannel(emitted, channels::tag(channels::Credit::refund)),
              1U);
  PL_CHECK_EQ(countChannel(emitted, channels::tag(channels::Credit::payment)),
              0U);
  PL_CHECK_EQ(ledger.cash(0), cardCashAfterPurchase + 100.0);

  std::puts("  PASS: an accepted merchant refund restores both card "
            "liquidity and the statement balance");
}

void testLatePaymentStaysOutOfEarlierStatement() {
  const credit_cards::BillingTerms billing{};
  credit_cards::PaymentBehavior payments{};
  payments.mixture.payFull = 1.0;
  payments.mixture.payPartial = 0.0;
  payments.mixture.payMin = 0.0;
  payments.mixture.miss = 0.0;
  payments.timing.lateProbability = 1.0;
  payments.timing.lateDaysMin = 20;
  payments.timing.lateDaysMax = 20;

  credit_cards::DisputeBehavior disputes{};
  disputes.rates.refundProbability = 0.0;
  disputes.rates.chargebackProbability = 0.0;

  auto factoryRng = random::Rng::fromSeed(1'010);
  const transactions::Factory factory(factoryRng);
  const credit_cards::detail::Environment env{billing, payments, disputes,
                                              factory, kIssuer};

  std::array<transactions::Transaction, 2> purchases{};
  purchases[0].source = kCard;
  purchases[0].target = kMerchant;
  purchases[0].amount = 100.0;
  purchases[0].timestamp = time::toEpochSeconds(at(2019, 1, 5, 14, 30));
  purchases[0].session.channel = channels::tag(channels::Legit::cardPurchase);
  purchases[1] = purchases[0];
  purchases[1].amount = 50.0;
  purchases[1].timestamp = time::toEpochSeconds(at(2019, 2, 5, 14, 30));

  const std::array<std::uint32_t, 2> indices{0, 1};
  std::vector<transactions::Transaction> emitted;
  credit_cards::detail::Session session(
      env,
      credit_cards::detail::Account{
          .card = kCard,
          .funding = kFunding,
          .apr = 0.20,
          .cycleDay = 10,
          .autopay = entity::card::Autopay::manual,
      },
      random::Rng::fromSeed(1'111), emitted);

  const credit_cards::detail::CardPurchases purchaseView{
      .txns = std::span<const transactions::Transaction>(purchases),
      .indices = std::span<const std::uint32_t>(indices),
  };
  session.run(purchaseView, {
                                .start = at(2019, 1, 1),
                                .endExcl = at(2019, 1, 10, 23, 30),
                                .windowEndExcl = at(2019, 5, 1),
                            });
  session.run(purchaseView, {
                                .start = at(2019, 1, 10, 23, 30),
                                .endExcl = at(2019, 2, 10, 23, 30),
                                .windowEndExcl = at(2019, 5, 1),
                            });

  std::array<double, 2> paymentAmounts{};
  std::size_t paymentCount = 0;
  const auto paymentTag = channels::tag(channels::Credit::payment);
  for (const auto &row : emitted) {
    if (row.session.channel == paymentTag) {
      PL_CHECK(paymentCount < paymentAmounts.size());
      paymentAmounts[paymentCount++] = row.amount;
    }
  }
  PL_CHECK_EQ(paymentCount, 2U);
  PL_CHECK_EQ(paymentAmounts[0], 100.0);
  PL_CHECK_EQ(paymentAmounts[1], 182.0);

  std::puts("  PASS: a payment scheduled after the next close does not "
            "reduce that earlier statement");
}

void testOutOfWindowLateFeeIsNotBackdated() {
  credit_cards::PaymentBehavior missed{};
  missed.mixture.payFull = 0.0;
  missed.mixture.payPartial = 0.0;
  missed.mixture.payMin = 0.0;
  missed.mixture.miss = 1.0;

  const credit_cards::BillingTerms billing{};
  credit_cards::DisputeBehavior disputes{};
  disputes.rates.refundProbability = 0.0;
  disputes.rates.chargebackProbability = 0.0;

  auto factoryRng = random::Rng::fromSeed(1'212);
  const transactions::Factory factory(factoryRng);
  const credit_cards::detail::Environment env{billing, missed, disputes,
                                              factory, kIssuer};

  transactions::Transaction purchase{};
  purchase.source = kCard;
  purchase.target = kMerchant;
  purchase.amount = 100.0;
  purchase.timestamp = time::toEpochSeconds(at(2019, 1, 5, 14, 30));
  purchase.session.channel = channels::tag(channels::Legit::cardPurchase);
  const std::array<transactions::Transaction, 1> purchases{purchase};
  const std::array<std::uint32_t, 1> indices{0};
  std::vector<transactions::Transaction> emitted;

  credit_cards::detail::Session session(
      env,
      credit_cards::detail::Account{
          .card = kCard,
          .funding = kFunding,
          .apr = 0.20,
          .cycleDay = 10,
          .autopay = entity::card::Autopay::manual,
      },
      random::Rng::fromSeed(1'313), emitted);
  session.run(
      {
          .txns = std::span<const transactions::Transaction>(purchases),
          .indices = std::span<const std::uint32_t>(indices),
      },
      {
          .start = at(2019, 1, 1),
          .endExcl = at(2019, 1, 10, 23, 30),
          .windowEndExcl = at(2019, 1, 20),
      });

  PL_CHECK_EQ(countChannel(emitted, channels::tag(channels::Credit::lateFee)),
              0U);
  std::puts("  PASS: a late fee due beyond the window is censored, never "
            "backdated to the final second");
}

} // namespace

int main() {
  testAutopayModesAreOnTime();
  testManualSamplerMatchesLateClassification();
  testManualLateFeeBehavior();
  testFuturePostingDoesNotTimeTravel();
  testRejectedPaymentDoesNotReduceStatement();
  testAcceptedRefundRestoresLedgerAndStatement();
  testLatePaymentStaysOutOfEarlierStatement();
  testOutOfWindowLateFeeIsNotBackdated();
  std::puts("test_card_payment_timing: all assertions passed");
  return 0;
}
