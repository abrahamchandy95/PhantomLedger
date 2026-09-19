#include "phantomledger/entities/counterparties/cash_points.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include "gate_world.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pl = ::PhantomLedger;
namespace legitLedger = pl::transfers::legit::ledger;

namespace {

constexpr double kTolerance = 1e-9;
constexpr double kRetiredHubSentinel = 1e18;

[[nodiscard]] bool nearlyEqual(double lhs, double rhs) noexcept {
  return std::abs(lhs - rhs) <= kTolerance;
}

[[nodiscard]] pl::transactions::Transaction
transaction(pl::entity::Key source, pl::entity::Key target, double amount,
            pl::channels::Tag channel, std::int64_t timestamp = 1) {
  pl::transactions::Transaction out{};
  out.source = source;
  out.target = target;
  out.amount = amount;
  out.timestamp = timestamp;
  out.session.channel = channel;
  return out;
}

[[nodiscard]] std::uint64_t
dropTotal(const legitLedger::ChronoReplayAccumulator &replay) {
  std::uint64_t out = 0;
  for (const auto &[reason, count] : replay.dropCounts()) {
    (void)reason;
    out += count;
  }
  return out;
}

struct BoundaryFixture {
  static constexpr pl::clearing::Ledger::Index customerSourceIx = 0;
  static constexpr pl::clearing::Ledger::Index customerDestinationIx = 1;
  static constexpr pl::clearing::Ledger::Index atmTerminalIx = 2;
  static constexpr pl::clearing::Ledger::Index cashDepositoryIx = 3;

  pl::entity::Key customerSource = pl::entity::makeKey(
      pl::entity::Role::account, pl::entity::Bank::internal, 101);
  pl::entity::Key customerDestination = pl::entity::makeKey(
      pl::entity::Role::account, pl::entity::Bank::internal, 102);
  pl::entity::Key atmTerminal = pl::entity::makeKey(
      pl::entity::Role::processor, pl::entity::Bank::external, 201);
  pl::entity::Key cashDepository = pl::entity::makeKey(
      pl::entity::Role::processor, pl::entity::Bank::external, 202);

  pl::clearing::Ledger book;
  pl::random::Rng rng = pl::random::Rng::fromSeed(0xCA5'B0A4DULL);

  BoundaryFixture() {
    book.initialize(4);
    book.addAccount(customerSource, customerSourceIx);
    book.addAccount(customerDestination, customerDestinationIx);

    // The boundary endpoints deliberately pass through addAccount: the
    // production registry contains them so transactions remain referentially
    // valid. Registration must never turn them into a balance-bearing leg.
    book.addAccount(
        atmTerminal, atmTerminalIx,
        pl::entity::boundary::Policy{
            .kind = pl::entity::boundary::Kind::atmTerminal,
            .allowedFlows = pl::entity::boundary::bit(
                pl::entity::boundary::Flow::outbound),
        });
    book.addAccount(
        cashDepository, cashDepositoryIx,
        pl::entity::boundary::Policy{
            .kind = pl::entity::boundary::Kind::cashDepository,
            .allowedFlows = pl::entity::boundary::bit(
                pl::entity::boundary::Flow::inbound),
        });

    book.cash(customerSourceIx) = 1'000.0;
    book.cash(customerDestinationIx) = 100.0;

    // Poison the external slots. A posting bug that resolves an external key
    // to its registered slot will visibly credit or debit these values.
    book.cash(atmTerminalIx) = 777.0;
    book.cash(cashDepositoryIx) = 888.0;
  }
};

void checkBoundaryBalances(BoundaryFixture &fixture, double source,
                           double destination, double terminal,
                           double depository) {
  auto &book = fixture.book;
  PL_CHECK(nearlyEqual(book.cash(BoundaryFixture::customerSourceIx), source));
  PL_CHECK(nearlyEqual(book.cash(BoundaryFixture::customerDestinationIx),
                       destination));
  PL_CHECK(nearlyEqual(book.cash(BoundaryFixture::atmTerminalIx), terminal));
  PL_CHECK(
      nearlyEqual(book.cash(BoundaryFixture::cashDepositoryIx), depository));
}

void testWithdrawalDebitsOnlyCustomer() {
  BoundaryFixture fixture;
  legitLedger::ChronoReplayAccumulator replay(
      &fixture.book, &fixture.rng,
      legitLedger::ChronoReplayAccumulator::defaultFundingBehavior(),
      /*emitLiquidityEvents=*/false);

  const auto withdrawal =
      transaction(fixture.customerSource, fixture.atmTerminal, 120.0,
                  pl::channels::tag(pl::channels::Legit::atm));

  PL_CHECK(replay.append(withdrawal));
  PL_CHECK_EQ(replay.txns().size(), 1U);
  PL_CHECK_EQ(dropTotal(replay), 0U);
  checkBoundaryBalances(fixture, 880.0, 100.0, 777.0, 888.0);

  std::printf("  PASS: internal -> external withdrawal is a one-sided debit\n");
}

void testCashDepositCreditsOnlyCustomer() {
  BoundaryFixture fixture;
  legitLedger::ChronoReplayAccumulator replay(
      &fixture.book, &fixture.rng,
      legitLedger::ChronoReplayAccumulator::defaultFundingBehavior(),
      /*emitLiquidityEvents=*/false);

  const auto deposit =
      transaction(fixture.cashDepository, fixture.customerDestination, 250.0,
                  pl::channels::tag(pl::channels::Legit::cashDeposit));

  PL_CHECK(replay.append(deposit));
  PL_CHECK_EQ(replay.txns().size(), 1U);
  PL_CHECK_EQ(dropTotal(replay), 0U);
  checkBoundaryBalances(fixture, 1'000.0, 350.0, 777.0, 888.0);

  std::printf(
      "  PASS: external -> internal cash deposit is a one-sided credit\n");
}

void testCheckAndCryptoBoundaryDirections() {
  const auto customer = pl::entity::makeKey(
      pl::entity::Role::account, pl::entity::Bank::internal, 301);
  const auto check = pl::counterparties::cash::checkCapture(1);
  const auto venue = pl::counterparties::cash::cryptoVenue(1);

  pl::clearing::Ledger book;
  book.initialize(3);
  book.addAccount(customer, 0);
  book.addAccount(
      check, 1,
      pl::entity::boundary::Policy{
          .kind = pl::entity::boundary::Kind::checkCapture,
          .allowedFlows = pl::entity::boundary::bit(
              pl::entity::boundary::Flow::inbound),
      });
  book.addAccount(venue, 2,
                  pl::entity::boundary::Policy{
                      .kind = pl::entity::boundary::Kind::cryptoVenue,
                      .allowedFlows = pl::entity::boundary::kBothFlows,
                  });
  book.cash(0) = 1'000.0;
  book.cash(1) = 777.0;
  book.cash(2) = 888.0;

  PL_CHECK(book.transfer(check, customer, 250.0,
                         pl::channels::Deposit::checkDeposit)
               .accepted());
  PL_CHECK(nearlyEqual(book.cash(0), 1'250.0));
  PL_CHECK(nearlyEqual(book.cash(1), 777.0));

  PL_CHECK(book.transfer(customer, venue, 200.0,
                         pl::channels::Crypto::rampOut)
               .accepted());
  PL_CHECK(nearlyEqual(book.cash(0), 1'050.0));
  PL_CHECK(nearlyEqual(book.cash(2), 888.0));

  PL_CHECK(book.transfer(venue, customer, 80.0,
                         pl::channels::Crypto::rampIn)
               .accepted());
  PL_CHECK(nearlyEqual(book.cash(0), 1'130.0));
  PL_CHECK(nearlyEqual(book.cash(2), 888.0));

  std::printf("  PASS: check and crypto boundaries mutate only the customer "
              "leg\n");
}

void testTypedBoundaryContractsRejectMisuse() {
  const auto customer = pl::entity::makeKey(
      pl::entity::Role::account, pl::entity::Bank::internal, 401);
  const auto atm = pl::counterparties::cash::atmTerminal(1);
  const auto cash = pl::counterparties::cash::depository(1);
  const auto generic = pl::entity::makeKey(
      pl::entity::Role::merchant, pl::entity::Bank::external, 99);

  pl::clearing::Ledger book;
  book.initialize(4);
  book.addAccount(customer, 0);
  book.addAccount(
      atm, 1,
      pl::entity::boundary::Policy{
          .kind = pl::entity::boundary::Kind::atmTerminal,
          .allowedFlows = pl::entity::boundary::bit(
              pl::entity::boundary::Flow::outbound),
      });
  book.addAccount(
      cash, 2,
      pl::entity::boundary::Policy{
          .kind = pl::entity::boundary::Kind::cashDepository,
          .allowedFlows = pl::entity::boundary::bit(
              pl::entity::boundary::Flow::inbound),
      });
  book.addAccount(generic, 3);
  book.cash(0) = 1'000.0;
  book.cash(1) = 111.0;
  book.cash(2) = 222.0;
  book.cash(3) = 333.0;

  PL_CHECK(book.transfer(atm, customer, 10.0, pl::channels::Legit::atm)
               .rejected());
  PL_CHECK(book.transfer(customer, cash, 10.0,
                         pl::channels::Legit::cashDeposit)
               .rejected());
  PL_CHECK(book.transfer(atm, customer, 10.0,
                         pl::channels::Legit::cashDeposit)
               .rejected());
  PL_CHECK(book.transfer(customer, atm, 10.0,
                         pl::channels::Legit::externalUnknown)
               .rejected());
  PL_CHECK(book.transfer(customer, generic, 10.0, pl::channels::Legit::atm)
               .rejected());

  PL_CHECK(nearlyEqual(book.cash(0), 1'000.0));
  PL_CHECK(nearlyEqual(book.cash(1), 111.0));
  PL_CHECK(nearlyEqual(book.cash(2), 222.0));
  PL_CHECK(nearlyEqual(book.cash(3), 333.0));

  std::printf("  PASS: wrong boundary kind, direction, and generic bypass "
              "are rejected\n");
}

void testExternalToExternalIsRejected() {
  BoundaryFixture fixture;
  legitLedger::ChronoReplayAccumulator replay(
      &fixture.book, &fixture.rng,
      legitLedger::ChronoReplayAccumulator::defaultFundingBehavior(),
      /*emitLiquidityEvents=*/false);

  const auto boundaryOnly =
      transaction(fixture.cashDepository, fixture.atmTerminal, 50.0,
                  pl::channels::tag(pl::channels::Legit::cashDeposit));

  PL_CHECK(!replay.append(boundaryOnly));
  PL_CHECK(replay.txns().empty());
  PL_CHECK_EQ(dropTotal(replay), 1U);
  checkBoundaryBalances(fixture, 1'000.0, 100.0, 777.0, 888.0);

  std::printf("  PASS: external -> external posting is rejected\n");
}

void testUnknownInternalEndpointsAreRejected() {
  BoundaryFixture fixture;
  legitLedger::ChronoReplayAccumulator replay(
      &fixture.book, &fixture.rng,
      legitLedger::ChronoReplayAccumulator::defaultFundingBehavior(),
      /*emitLiquidityEvents=*/false);

  const auto unknown = pl::entity::makeKey(pl::entity::Role::account,
                                           pl::entity::Bank::internal, 999'999);
  PL_CHECK_EQ(fixture.book.findAccount(unknown), pl::clearing::Ledger::invalid);

  const auto unknownSource =
      transaction(unknown, fixture.customerDestination, 25.0,
                  pl::channels::tag(pl::channels::Legit::selfTransfer));
  const auto unknownDestination =
      transaction(fixture.customerSource, unknown, 25.0,
                  pl::channels::tag(pl::channels::Legit::selfTransfer), 2);

  PL_CHECK(!replay.append(unknownSource));
  PL_CHECK(!replay.append(unknownDestination));
  PL_CHECK(replay.txns().empty());
  PL_CHECK_EQ(dropTotal(replay), 2U);
  checkBoundaryBalances(fixture, 1'000.0, 100.0, 777.0, 888.0);

  std::printf("  PASS: unknown internal endpoints are rejected\n");
}

void testUnknownExternalEndpointsAreRejected() {
  BoundaryFixture fixture;
  legitLedger::ChronoReplayAccumulator replay(
      &fixture.book, &fixture.rng,
      legitLedger::ChronoReplayAccumulator::defaultFundingBehavior(),
      /*emitLiquidityEvents=*/false);

  const auto unknown = pl::entity::makeKey(pl::entity::Role::processor,
                                           pl::entity::Bank::external, 999'999);
  const auto invalid = pl::entity::makeKey(pl::entity::Role::processor,
                                           pl::entity::Bank::external, 0);

  PL_CHECK(!replay.append(
      transaction(unknown, fixture.customerDestination, 25.0,
                  pl::channels::tag(pl::channels::Legit::cashDeposit))));
  PL_CHECK(!replay.append(
      transaction(fixture.customerSource, unknown, 25.0,
                  pl::channels::tag(pl::channels::Legit::atm), 2)));
  PL_CHECK(!replay.append(
      transaction(invalid, fixture.customerDestination, 25.0,
                  pl::channels::tag(pl::channels::Legit::cashDeposit), 3)));
  PL_CHECK(!replay.append(
      transaction(fixture.customerSource, invalid, 25.0,
                  pl::channels::tag(pl::channels::Legit::atm), 4)));

  PL_CHECK(replay.txns().empty());
  PL_CHECK_EQ(dropTotal(replay), 4U);
  checkBoundaryBalances(fixture, 1'000.0, 100.0, 777.0, 888.0);

  std::printf("  PASS: unknown and invalid external endpoints are rejected\n");
}

void checkFiniteLedger(pl::clearing::Ledger &book) {
  PL_CHECK(book.size() > 0U);
  for (pl::clearing::Ledger::Index idx = 0; idx < book.size(); ++idx) {
    const auto cash = book.cash(idx);
    const auto overdraft = book.overdraft(idx);
    const auto linked = book.linked(idx);
    const auto courtesy = book.courtesy(idx);

    PL_CHECK(std::isfinite(cash));
    PL_CHECK(std::isfinite(overdraft));
    PL_CHECK(std::isfinite(linked));
    PL_CHECK(std::isfinite(courtesy));
    PL_CHECK(std::isfinite(book.availableCash(idx)));
    PL_CHECK(std::isfinite(book.liquidity(idx)));
    PL_CHECK(cash != kRetiredHubSentinel);
  }
}

void testCsvRejectsPositiveInfinity() {
  std::ostringstream output;
  pl::exporter::csv::Writer writer(output);

  PL_CHECK_THROWS(writer.cell(std::numeric_limits<double>::infinity()));
  PL_CHECK(output.str().empty());

  std::printf("  PASS: CSV writer rejects positive infinity\n");
}

void checkEndpointRecord(const pltest::GateWorld &world,
                         const std::unordered_set<pl::entity::Key> &owned,
                         pl::entity::Key endpoint) {
  PL_CHECK(endpoint.bank == pl::entity::Bank::external);
  PL_CHECK(!owned.contains(endpoint));

  const auto found = world.holdings.accounts.lookup.byId.find(endpoint);
  PL_CHECK(found != world.holdings.accounts.lookup.byId.end());
  PL_CHECK(found->second < world.holdings.accounts.registry.records.size());

  const auto &record = world.holdings.accounts.registry.records[found->second];
  PL_CHECK(record.id == endpoint);
  PL_CHECK_EQ(record.owner, pl::entity::invalidPerson);
  PL_CHECK(pl::entity::account::hasFlag(record.flags,
                                        pl::entity::account::Flag::external));
}

void checkBoundaryRecord(const pltest::GateWorld &world,
                         const std::unordered_set<pl::entity::Key> &owned,
                         pl::entity::Key endpoint,
                         pl::entity::boundary::Kind kind,
                         pl::entity::boundary::Flow requiredFlow) {
  checkEndpointRecord(world, owned, endpoint);
  const auto found = world.holdings.accounts.lookup.byId.find(endpoint);
  PL_CHECK(found != world.holdings.accounts.lookup.byId.end());
  const auto &record = world.holdings.accounts.registry.records[found->second];
  PL_CHECK_EQ(record.boundaryPolicy.kind, kind);
  PL_CHECK(pl::entity::boundary::allows(record.boundaryPolicy, requiredFlow));
}

void testEmptyConfiguredPoolsUseRegisteredFallbacks() {
  namespace cash = pl::counterparties::cash;
  constexpr std::uint64_t seed = 0xFA11'BAC0ULL;

  pl::time::Window window{};
  window.start = pl::time::makeTime({2021, 1, 1});
  window.days = 365;

  pl::synth::counterparties::CounterpartyTargets targets{};
  targets.external.atmTerminals = {};
  targets.external.cashDepositories = {};
  targets.external.checkCapturePoints = {};
  targets.external.cryptoVenues = {};
  targets.external.billers = {};

  const auto pools = pltest::buildPoolSet(seed);
  pltest::WorldSpec spec{};
  spec.seed = seed;
  spec.window = window;
  spec.population = 300;
  spec.counterpartyTargets = targets;
  spec.withProducts = false;
  spec.withInfra = false;
  spec.withInfraRouting = false;
  spec.withIncome = true;
  spec.withBaseRoutines = true;

  pltest::GateWorld world(pools, spec);
  PL_CHECK(world.cps.counterparties.external.atmTerminals.empty());
  PL_CHECK(world.cps.counterparties.external.cashDepositories.empty());
  PL_CHECK(world.cps.counterparties.external.checkCapturePoints.empty());
  PL_CHECK(world.cps.counterparties.external.cryptoVenues.empty());
  PL_CHECK(world.cps.counterparties.external.billers.empty());

  const auto &counterparties = world.plan.counterparties();
  PL_CHECK_EQ(counterparties.cashWithdrawalPoints.size(), 2U);
  PL_CHECK_EQ(counterparties.cashDepositPoints.size(), 2U);
  PL_CHECK_EQ(counterparties.checkDepositPoints.size(), 2U);
  PL_CHECK_EQ(counterparties.cryptoVenues.size(), 4U);
  PL_CHECK_EQ(counterparties.billerAccounts.size(), 8U);
  PL_CHECK_EQ(counterparties.cashWithdrawalPoints[0], cash::atmTerminal(1));
  PL_CHECK_EQ(counterparties.cashWithdrawalPoints[1], cash::atmTerminal(2));
  PL_CHECK_EQ(counterparties.cashDepositPoints[0], cash::depository(1));
  PL_CHECK_EQ(counterparties.cashDepositPoints[1], cash::depository(2));
  PL_CHECK_EQ(counterparties.checkDepositPoints[0], cash::checkCapture(1));
  PL_CHECK_EQ(counterparties.checkDepositPoints[1], cash::checkCapture(2));
  PL_CHECK_EQ(counterparties.cryptoVenues[0], cash::cryptoVenue(1));
  PL_CHECK_EQ(counterparties.cryptoVenues[3], cash::cryptoVenue(4));

  std::unordered_set<pl::entity::Key> owned;
  for (const auto &record : world.holdings.accounts.registry.records) {
    if (record.owner != pl::entity::invalidPerson) {
      owned.insert(record.id);
    }
  }
  for (const auto endpoint : counterparties.cashWithdrawalPoints) {
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::atmTerminal,
                        pl::entity::boundary::Flow::outbound);
  }
  for (const auto endpoint : counterparties.cashDepositPoints) {
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::cashDepository,
                        pl::entity::boundary::Flow::inbound);
  }
  for (const auto endpoint : counterparties.checkDepositPoints) {
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::checkCapture,
                        pl::entity::boundary::Flow::inbound);
  }
  for (const auto endpoint : counterparties.cryptoVenues) {
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::cryptoVenue,
                        pl::entity::boundary::Flow::outbound);
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::cryptoVenue,
                        pl::entity::boundary::Flow::inbound);
  }
  for (const auto endpoint : counterparties.billerAccounts) {
    checkEndpointRecord(world, owned, endpoint);
  }

  const std::unordered_set<pl::entity::Key> atmSet(
      counterparties.cashWithdrawalPoints.begin(),
      counterparties.cashWithdrawalPoints.end());
  const std::unordered_set<pl::entity::Key> billerSet(
      counterparties.billerAccounts.begin(),
      counterparties.billerAccounts.end());
  const std::unordered_set<pl::entity::Key> cashDepositSet(
      counterparties.cashDepositPoints.begin(),
      counterparties.cashDepositPoints.end());
  const std::unordered_set<pl::entity::Key> checkDepositSet(
      counterparties.checkDepositPoints.begin(),
      counterparties.checkDepositPoints.end());
  const std::unordered_set<pl::entity::Key> cryptoSet(
      counterparties.cryptoVenues.begin(), counterparties.cryptoVenues.end());
  std::size_t atmRows = 0;
  std::size_t cashDepositRows = 0;
  std::size_t checkDepositRows = 0;
  std::size_t cryptoOutRows = 0;
  std::size_t subscriptionRows = 0;
  for (const auto &txn : world.streams.screened()) {
    if (pl::channels::is(txn.session.channel, pl::channels::Legit::atm)) {
      ++atmRows;
      PL_CHECK(atmSet.contains(txn.target));
    }
    if (pl::channels::is(txn.session.channel,
                         pl::channels::Legit::subscription)) {
      ++subscriptionRows;
      PL_CHECK(billerSet.contains(txn.target));
    }
    if (pl::channels::is(txn.session.channel,
                         pl::channels::Legit::cashDeposit)) {
      ++cashDepositRows;
      PL_CHECK(cashDepositSet.contains(txn.source));
      PL_CHECK(owned.contains(txn.target));
    }
    if (pl::channels::is(txn.session.channel,
                         pl::channels::Deposit::checkDeposit)) {
      ++checkDepositRows;
      PL_CHECK(checkDepositSet.contains(txn.source));
      PL_CHECK(owned.contains(txn.target));
    }
    if (pl::channels::is(txn.session.channel,
                         pl::channels::Crypto::rampOut)) {
      ++cryptoOutRows;
      PL_CHECK(owned.contains(txn.source));
      PL_CHECK(cryptoSet.contains(txn.target));
    }
  }
  PL_CHECK(atmRows > 0U);
  PL_CHECK(cashDepositRows > 0U);
  PL_CHECK(checkDepositRows > 0U);
  PL_CHECK(cryptoOutRows > 0U);
  PL_CHECK(subscriptionRows > 0U);

  auto finalBook = world.initialBook->clone();
  auto replayRng = pl::random::Rng::fromSeed(seed ^ 0xB00C'DA4EULL);
  legitLedger::ChronoReplayAccumulator replay(
      &finalBook, &replayRng,
      legitLedger::ChronoReplayAccumulator::defaultFundingBehavior(),
      /*emitLiquidityEvents=*/false);
  auto replayRows = world.streams.replayReady();
  replay.extend(std::move(replayRows), /*presorted=*/true);
  PL_CHECK(replay.dropCounts().find(legitLedger::drop_reasons::kUnbooked) ==
           replay.dropCounts().end());

  std::printf("  PASS: empty configured pools use registered fallback "
              "endpoints without unbooked drops\n");
}

void testGeneratedWorldCashBoundaries() {
  constexpr std::uint64_t seed = 0xCA5'2026ULL;

  pl::time::Window window{};
  window.start = pl::time::makeTime({2019, 1, 1});
  window.days = 730;

  const auto pools = pltest::buildPoolSet(seed);
  pltest::WorldSpec spec{};
  spec.seed = seed;
  spec.window = window;
  spec.population = 300;
  spec.withProducts = false;
  spec.withInfra = false;
  spec.withInfraRouting = false;
  spec.withIncome = true;
  spec.withBaseRoutines = true;

  pltest::GateWorld world(pools, spec);
  PL_CHECK(world.initialBook != nullptr);
  checkFiniteLedger(*world.initialBook);

  std::unordered_set<pl::entity::Key> owned;
  owned.reserve(world.holdings.accounts.registry.records.size());
  for (const auto &record : world.holdings.accounts.registry.records) {
    if (record.owner != pl::entity::invalidPerson) {
      owned.insert(record.id);
    }
  }
  PL_CHECK(!owned.empty());

  const auto &configured = world.plan.counterparties().cashWithdrawalPoints;
  PL_CHECK(configured.size() >= 2U);
  PL_CHECK_EQ(world.cps.counterparties.external.atmTerminalAreas.size(),
              configured.size());

  std::unordered_set<pl::entity::Key> configuredSet(configured.begin(),
                                                    configured.end());
  PL_CHECK_EQ(configuredSet.size(), configured.size());
  for (const auto endpoint : configured) {
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::atmTerminal,
                        pl::entity::boundary::Flow::outbound);
  }

  const auto &cashDeposits = world.plan.counterparties().cashDepositPoints;
  const auto &checkDeposits = world.plan.counterparties().checkDepositPoints;
  const auto &cryptoVenues = world.plan.counterparties().cryptoVenues;
  const std::unordered_set<pl::entity::Key> cashDepositSet(
      cashDeposits.begin(), cashDeposits.end());
  const std::unordered_set<pl::entity::Key> checkDepositSet(
      checkDeposits.begin(), checkDeposits.end());
  const std::unordered_set<pl::entity::Key> cryptoVenueSet(
      cryptoVenues.begin(), cryptoVenues.end());
  for (const auto endpoint : cashDeposits) {
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::cashDepository,
                        pl::entity::boundary::Flow::inbound);
  }
  for (const auto endpoint : checkDeposits) {
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::checkCapture,
                        pl::entity::boundary::Flow::inbound);
  }
  for (const auto endpoint : cryptoVenues) {
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::cryptoVenue,
                        pl::entity::boundary::Flow::outbound);
    checkBoundaryRecord(world, owned, endpoint,
                        pl::entity::boundary::Kind::cryptoVenue,
                        pl::entity::boundary::Flow::inbound);
  }

  std::unordered_map<pl::entity::Key, std::size_t> observed;
  std::unordered_map<pl::entity::Key, double> cryptoOutByAccount;
  std::unordered_map<pl::entity::Key, double> cryptoInByAccount;
  std::size_t atmRows = 0;
  std::size_t cashDepositRows = 0;
  std::size_t checkDepositRows = 0;
  std::size_t cryptoOutRows = 0;
  std::size_t cryptoInRows = 0;
  for (const auto &txn : world.streams.screened()) {
    if (pl::channels::is(txn.session.channel, pl::channels::Legit::atm)) {
      ++atmRows;
      PL_CHECK(configuredSet.contains(txn.target));
      checkEndpointRecord(world, owned, txn.target);

      const auto source = world.holdings.accounts.lookup.byId.find(txn.source);
      PL_CHECK(source != world.holdings.accounts.lookup.byId.end());
      const auto &sourceRecord =
          world.holdings.accounts.registry.records[source->second];
      PL_CHECK(sourceRecord.owner != pl::entity::invalidPerson);
      PL_CHECK(!pl::entity::account::hasFlag(
          sourceRecord.flags, pl::entity::account::Flag::external));
      PL_CHECK(owned.contains(txn.source));

      const auto local = world.plan.counterparties().withdrawalPointsFor(
          sourceRecord.owner, txn.timestamp);
      PL_CHECK(!local.empty());
      PL_CHECK(local.size() <= 4U);
      PL_CHECK(std::ranges::find(local, txn.target) != local.end());
      ++observed[txn.target];
      continue;
    }

    if (pl::channels::is(txn.session.channel,
                         pl::channels::Legit::cashDeposit)) {
      ++cashDepositRows;
      PL_CHECK(cashDepositSet.contains(txn.source));
      PL_CHECK(owned.contains(txn.target));
      const auto target = world.holdings.accounts.lookup.byId.find(txn.target);
      PL_CHECK(target != world.holdings.accounts.lookup.byId.end());
      const auto owner =
          world.holdings.accounts.registry.records[target->second].owner;
      const auto local = world.plan.counterparties().depositPointsFor(
          owner, txn.timestamp);
      PL_CHECK(!local.empty());
      PL_CHECK(local.size() <= 4U);
      PL_CHECK(std::ranges::find(local, txn.source) != local.end());
      continue;
    }

    if (pl::channels::is(txn.session.channel,
                         pl::channels::Deposit::checkDeposit)) {
      ++checkDepositRows;
      PL_CHECK(checkDepositSet.contains(txn.source));
      PL_CHECK(owned.contains(txn.target));
      const auto target = world.holdings.accounts.lookup.byId.find(txn.target);
      PL_CHECK(target != world.holdings.accounts.lookup.byId.end());
      const auto owner =
          world.holdings.accounts.registry.records[target->second].owner;
      const auto local = world.plan.counterparties().checkDepositPointsFor(
          owner, txn.timestamp);
      PL_CHECK(!local.empty());
      PL_CHECK(local.size() <= 4U);
      PL_CHECK(std::ranges::find(local, txn.source) != local.end());
      continue;
    }

    if (pl::channels::is(txn.session.channel,
                         pl::channels::Crypto::rampOut)) {
      ++cryptoOutRows;
      PL_CHECK(owned.contains(txn.source));
      PL_CHECK(cryptoVenueSet.contains(txn.target));
      cryptoOutByAccount[txn.source] += txn.amount;
      continue;
    }

    if (pl::channels::is(txn.session.channel,
                         pl::channels::Crypto::rampIn)) {
      ++cryptoInRows;
      PL_CHECK(cryptoVenueSet.contains(txn.source));
      PL_CHECK(owned.contains(txn.target));
      cryptoInByAccount[txn.target] += txn.amount;
    }
  }

  PL_CHECK(atmRows >= 20U);
  PL_CHECK(observed.size() >= 2U);
  const auto busiest = std::max_element(
      observed.begin(), observed.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.second < rhs.second; });
  PL_CHECK(busiest != observed.end());
  PL_CHECK(busiest->second * 100U <= atmRows * 95U);
  PL_CHECK(cashDepositRows > 0U);
  PL_CHECK(checkDepositRows > 0U);
  PL_CHECK(cryptoOutRows > 0U);
  for (const auto &[account, returned] : cryptoInByAccount) {
    PL_CHECK(returned <= cryptoOutByAccount[account] + kTolerance);
  }

  auto finalBook = world.initialBook->clone();
  auto replayRng = pl::random::Rng::fromSeed(seed ^ 0x5E77'1EULL);
  legitLedger::ChronoReplayAccumulator replay(
      &finalBook, &replayRng,
      legitLedger::ChronoReplayAccumulator::defaultFundingBehavior(),
      /*emitLiquidityEvents=*/false);
  auto replayRows = world.streams.replayReady();
  replay.extend(std::move(replayRows), /*presorted=*/true);
  PL_CHECK(!replay.txns().empty());
  checkFiniteLedger(finalBook);

  std::printf("  PASS: generated world has %zu ATM rows across %zu/%zu "
              "external, ownerless terminals\n",
              atmRows, observed.size(), configured.size());
  std::printf("  PASS: generated world has %zu cash deposits, %zu check "
              "deposits, and %zu/%zu crypto ramp-out/ramp-in rows\n",
              cashDepositRows, checkDepositRows, cryptoOutRows, cryptoInRows);
  std::printf("  PASS: opening and replayed ledger values are finite\n");
}

} // namespace

int main() {
  std::printf("=== Cash Boundary Regression Tests ===\n");
  testWithdrawalDebitsOnlyCustomer();
  testCashDepositCreditsOnlyCustomer();
  testCheckAndCryptoBoundaryDirections();
  testTypedBoundaryContractsRejectMisuse();
  testExternalToExternalIsRejected();
  testUnknownInternalEndpointsAreRejected();
  testUnknownExternalEndpointsAreRejected();
  testCsvRejectsPositiveInfinity();
  testEmptyConfiguredPoolsUseRegisteredFallbacks();
  testGeneratedWorldCashBoundaries();
  std::printf("All cash boundary regression tests passed.\n");
  return 0;
}
