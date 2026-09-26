#include "phantomledger/activity/income/types.hpp"
#include "phantomledger/entities/counterparties/cash_points.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/counterparties/make.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/geo/residence.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include "gate_world.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <sstream>
#include <string>
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

// The former cash-point selection (hub-realism-2026-09, Change 4), kept as the
// reference arm: rank by (distance, pool index) and take the first
// kNearbyCount. An area the catalogue does not hold got no entry and used the
// whole pool, as here.
[[nodiscard]] std::vector<pl::entity::Key>
indexTieBreak(std::span<const pl::entity::Key> points,
              std::span<const pl::entity::geography::GeoAreaId> at,
              pl::entity::geography::GeoAreaId home) {
  const auto &geo = pl::synth::geo::geography();
  std::vector<std::pair<double, std::size_t>> ranked;
  if (geo.contains(home)) {
    ranked.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
      if (geo.contains(at[i])) {
        ranked.emplace_back(
            pl::entity::geography::distanceMiles(geo.at(home), geo.at(at[i])),
            i);
      }
    }
  }
  if (ranked.empty()) {
    return {points.begin(), points.end()};
  }
  std::ranges::sort(ranked);
  std::vector<pl::entity::Key> out;
  for (std::size_t i = 0;
       i < std::min(pl::counterparties::cash::kNearbyCount, ranked.size());
       ++i) {
    out.push_back(points[ranked[i].second]);
  }
  return out;
}

[[nodiscard]] std::vector<pl::entity::Key>
keysOf(const pl::counterparties::cash::LocalPoints &local) {
  const auto span = local.span();
  return {span.begin(), span.end()};
}

// Rails in pool order: ATM, cash deposit, check deposit.
inline constexpr std::size_t kCorpusRails = 3;
inline constexpr std::array<const char *, kCorpusRails> kCorpusRailNames{
    "ATM", "cash-deposit", "check-deposit"};

// One generated corpus. At two points per rail every owner's set is the whole
// pool, so membership cannot exclude a point; the larger legs run the real
// emitters (atm.cpp, deposits.cpp and the revenue book's cash takings) where
// it can.
struct CorpusLeg {
  std::uint64_t seed = 0;
  pl::time::Window window{};
  std::int32_t population = 0;
  // Per rail: the pool must exceed kNearbyCount, so every row's owner holds a
  // set smaller than the pool and the membership check can fail.
  std::array<bool, kCorpusRails> narrowed{};
  // Per rail: some row's endpoint must lie outside the former list for the
  // same area, a row only the per-person tie-break window can produce.
  std::array<bool, kCorpusRails> moved{};
  // Crypto ramps open in 2013 (crypto.hpp, firstActivityYear), so a window
  // before it has none.
  bool cryptoRows = false;
};

struct CorpusRail {
  std::size_t rows = 0;
  // Rows whose owner's set is smaller than the pool.
  std::size_t narrowed = 0;
  // Rows whose endpoint the former list for the same area does not hold.
  std::size_t moved = 0;
};

void runGeneratedWorldLeg(const CorpusLeg &leg) {
  namespace cash = pl::counterparties::cash;
  const auto seed = leg.seed;

  const auto pools = pltest::buildPoolSet(seed);
  pltest::WorldSpec spec{};
  spec.seed = seed;
  spec.window = leg.window;
  spec.population = leg.population;
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

  const auto &access = world.plan.counterparties();
  const auto &external = world.cps.counterparties.external;
  PL_CHECK_EQ(external.cashDepositoryAreas.size(), cashDeposits.size());
  PL_CHECK_EQ(external.checkCaptureAreas.size(), checkDeposits.size());
  const std::array<std::span<const pl::entity::Key>, kCorpusRails> railPools{
      configured, cashDeposits, checkDeposits};
  const std::array<std::span<const pl::entity::geography::GeoAreaId>,
                   kCorpusRails>
      railAreas{external.atmTerminalAreas, external.cashDepositoryAreas,
                external.checkCaptureAreas};

  // The area the lookup resolves at event time (plans.hpp, localPointsFor).
  const auto areaAt = [&](pl::entity::PersonId person, std::int64_t ts) {
    auto area = pl::entity::geography::invalidGeoArea;
    if (person != pl::entity::invalidPerson &&
        person <= access.homeAreas.size()) {
      area = access.homeAreas[person - 1U];
    }
    if (access.relocation != nullptr) {
      const auto atDate = access.relocation->areaAt(person, ts);
      if (pl::entity::geography::validArea(atDate)) {
        area = atDate;
      }
    }
    return area;
  };
  const auto monthStartAt = [&](std::int64_t ts) {
    auto out = ts;
    for (const auto start : world.plan.monthStarts()) {
      if (const auto epoch = pl::time::toEpochSeconds(start); epoch <= ts) {
        out = epoch;
      }
    }
    return out;
  };

  std::array<CorpusRail, kCorpusRails> rails{};
  std::array<std::unordered_map<pl::entity::geography::GeoAreaId,
                                std::vector<pl::entity::Key>>,
             kCorpusRails>
      former;
  // The domain predicate on every cash-point row: the endpoint lies in its
  // owner's own set at the resolving time, whose size is min(4, pool).
  const auto checkLocal = [&](std::size_t rail, const cash::LocalPoints &local,
                              pl::entity::Key endpoint,
                              pl::entity::PersonId owner, std::int64_t ts) {
    const auto pool = railPools[rail];
    PL_CHECK_EQ(local.size(), std::min(cash::kNearbyCount, pool.size()));
    PL_CHECK(std::ranges::find(local.span(), endpoint) != local.span().end());
    auto &out = rails[rail];
    ++out.rows;
    out.narrowed += local.size() < pool.size() ? 1U : 0U;
    const auto area = areaAt(owner, ts);
    auto it = former[rail].find(area);
    if (it == former[rail].end()) {
      it = former[rail]
               .emplace(area, indexTieBreak(pool, railAreas[rail], area))
               .first;
    }
    out.moved += std::ranges::find(it->second, endpoint) == it->second.end()
                     ? 1U
                     : 0U;
  };
  std::size_t monthStartRows = 0;

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

      const auto local =
          access.withdrawalPointsFor(sourceRecord.owner, txn.timestamp);
      checkLocal(0, local, txn.target, sourceRecord.owner, txn.timestamp);
      // atm.cpp's exact pick: terminalFor inside the owner's own set.
      PL_CHECK(txn.target ==
               cash::terminalFor(local.span(), txn.source, txn.timestamp));
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
      auto at = txn.timestamp;
      auto local = access.depositPointsFor(owner, at);
      if (std::ranges::find(local.span(), txn.source) == local.span().end()) {
        // Business cash takings resolve the owner's set once, at the month
        // start (revenue/generate.hpp), so a mid-month mover keeps that set.
        at = monthStartAt(txn.timestamp);
        local = access.depositPointsFor(owner, at);
        ++monthStartRows;
      }
      checkLocal(1, local, txn.source, owner, at);
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
      const auto local = access.checkDepositPointsFor(owner, txn.timestamp);
      checkLocal(2, local, txn.source, owner, txn.timestamp);
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
  if (leg.cryptoRows) {
    PL_CHECK(cryptoOutRows > 0U);
  }
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

  for (std::size_t r = 0; r < kCorpusRails; ++r) {
    if (leg.narrowed[r]) {
      PL_CHECK(railPools[r].size() > cash::kNearbyCount);
      PL_CHECK(rails[r].narrowed > 0U);
    }
    if (leg.moved[r]) {
      PL_CHECK(rails[r].moved > 0U);
    }
  }

  std::printf("  PASS: pop %d world has %zu ATM rows across %zu/%zu "
              "external, ownerless terminals\n",
              leg.population, atmRows, observed.size(), configured.size());
  std::printf("  PASS: pop %d world has %zu cash deposits, %zu check "
              "deposits, and %zu/%zu crypto ramp-out/ramp-in rows\n",
              leg.population, cashDepositRows, checkDepositRows,
              cryptoOutRows, cryptoInRows);
  for (std::size_t r = 0; r < kCorpusRails; ++r) {
    std::printf("  PASS: pop %d %s rows lie in their owner's own set: %zu "
                "rows, %zu with a set smaller than the %zu-point pool, %zu "
                "outside the former index tie-break list\n",
                leg.population, kCorpusRailNames[r], rails[r].rows,
                rails[r].narrowed, railPools[r].size(), rails[r].moved);
  }
  std::printf("  PASS: pop %d %zu cash deposits resolved at the month start "
              "(business takings of a mid-month mover)\n",
              leg.population, monthStartRows);
  std::printf("  PASS: opening and replayed ledger values are finite\n");
}

[[nodiscard]] pl::time::Window corpusWindow(int year, int days) {
  pl::time::Window window{};
  window.start = pl::time::makeTime({year, 1, 1});
  window.days = days;
  return window;
}

void testGeneratedWorldCashBoundaries() {
  runGeneratedWorldLeg({.seed = 0xCA5'2026ULL,
                        .window = corpusWindow(2019, 730),
                        .population = 300,
                        .cryptoRows = true});
  // The AML table golden's world (test_table_golden: aml-txn-edges, pop
  // 10,000, 60 days from 1991-01-01, seed 7). Its 14 ATMs put some areas'
  // four-point cut inside a tie group, which is what moves that golden; its 3
  // depositories and 3 check-capture points still fit the cut.
  runGeneratedWorldLeg({.seed = 7,
                        .window = corpusWindow(1991, 60),
                        .population = 10'000,
                        .narrowed = {true, false, false},
                        .moved = {true, false, false}});
  // At pop 20,000 there are 5 depositories and 5 check-capture points, so
  // household deposits, check deposits and business cash takings also pick
  // inside a set smaller than their pool.
  runGeneratedWorldLeg({.seed = 7,
                        .window = corpusWindow(1991, 60),
                        .population = 20'000,
                        .narrowed = {true, true, true},
                        .moved = {true, false, false}});
}

// ---------------------------------------------------------------------------
// hub-realism-2026-09, Change 4: every service point of a city is used.
//
// Residents sit at their area's centroid, so every point placed in their own
// area ties at distance zero. The former builder broke that tie by pool index
// and handed the whole city the same lowest-numbered four points: at pop
// 200,000 New York had 41 ATMs and used 4, and the busiest terminal took about
// 300,000 withdrawals a year. The unit test pins the per-person window on
// hand-built directories. The scale gate drives the real directory builder,
// residence model, blueprint lookup and pickers at the populations where the
// concentration is large; the corpus legs above run the real emitters at pop
// 10,000 and 20,000, where the per-person set is smaller than the pool.

namespace nearby {

namespace cash = pl::counterparties::cash;
namespace ge = pl::entity::geography;
namespace blueprints = pl::transfers::legit::blueprints;
using Key = pl::entity::Key;

[[nodiscard]] ge::GeoAreaId areaNamed(std::string_view city) {
  for (const auto &area : pl::synth::geo::geography().areas()) {
    if (area.city == city) {
      return area.id;
    }
  }
  PL_CHECK(false);
  return ge::invalidGeoArea;
}

// Places `count` points of every rail in `area`, continuing each pool's
// ordinals.
void place(pl::entity::counterparty::Directory &directory, ge::GeoAreaId area,
           std::size_t count) {
  auto &ext = directory.external;
  for (std::size_t i = 0; i < count; ++i) {
    ext.atmTerminals.push_back(cash::atmTerminal(ext.atmTerminals.size() + 1));
    ext.atmTerminalAreas.push_back(area);
    ext.cashDepositories.push_back(
        cash::depository(ext.cashDepositories.size() + 1));
    ext.cashDepositoryAreas.push_back(area);
    ext.checkCapturePoints.push_back(
        cash::checkCapture(ext.checkCapturePoints.size() + 1));
    ext.checkCaptureAreas.push_back(area);
  }
}

// A blueprint with no persons burns nothing, so any draw here would be the
// nearby-set build itself. It must draw nothing.
[[nodiscard]] blueprints::LegitBlueprint
buildPlan(const pl::entity::counterparty::Directory *directory,
          std::span<const ge::GeoAreaId> homes) {
  blueprints::LegitBlueprint plan;
  auto rng = pl::random::Rng::fromSeed(0x4E45'4152'4259ULL);
  auto untouched = rng;
  plan.addCounterparties(rng, blueprints::CounterpartyPools{
                                  .directory = directory,
                                  .landlords = nullptr,
                                  .homeAreas = homes,
                                  .relocation = nullptr,
                              });
  PL_CHECK_EQ(rng.nextU64(), untouched.nextU64());
  return plan;
}

void testNearbySetsSpreadTiedPoints() {
  const auto newYork = areaNamed("New York");
  const auto newark = areaNamed("Newark");
  constexpr std::size_t kResidents = 2'000;
  constexpr std::size_t kCityPoints = 12;

  // A1: twelve points of every rail in New York, 2,000 New York residents.
  pl::entity::counterparty::Directory city;
  place(city, newYork, kCityPoints);
  const std::vector<ge::GeoAreaId> cityHomes(kResidents, newYork);
  const auto cityPlan = buildPlan(&city, cityHomes);
  const auto &cityAccess = cityPlan.counterparties();

  std::unordered_map<Key, std::size_t> appearances;
  std::unordered_set<Key> referenceUnion;
  for (pl::entity::PersonId p = 1; p <= kResidents; ++p) {
    const auto local = cityAccess.withdrawalPointsFor(p, 0);
    PL_CHECK_EQ(local.size(), cash::kNearbyCount);
    const std::unordered_set<Key> distinct(local.span().begin(),
                                           local.span().end());
    PL_CHECK_EQ(distinct.size(), cash::kNearbyCount);
    for (const auto key : local.span()) {
      ++appearances[key];
    }
    for (const auto key : indexTieBreak(city.external.atmTerminals,
                                        city.external.atmTerminalAreas,
                                        newYork)) {
      referenceUnion.insert(key);
    }
  }
  PL_CHECK_EQ(appearances.size(), kCityPoints);
  PL_CHECK_EQ(referenceUnion.size(), cash::kNearbyCount);
  const double fair = static_cast<double>(kResidents * cash::kNearbyCount) /
                      static_cast<double>(kCityPoints);
  std::size_t fewest = kResidents;
  std::size_t most = 0;
  for (const auto &[key, count] : appearances) {
    (void)key;
    fewest = std::min(fewest, count);
    most = std::max(most, count);
    PL_CHECK(static_cast<double>(count) >= 0.75 * fair);
    PL_CHECK(static_cast<double>(count) <= 1.25 * fair);
  }
  std::printf("  PASS: A1 %zu tied points, all used (the index tie-break "
              "uses %zu); sets per point %zu..%zu against a fair %.1f\n",
              appearances.size(), referenceUnion.size(), fewest, most, fair);

  // A2 and A5: one Newark point plus the twelve New York points. The Newark
  // point is strictly nearer to a Newark resident, so it is always kept; the
  // other three come from the tied New York group.
  pl::entity::counterparty::Directory metro;
  place(metro, newark, 1);
  place(metro, newYork, kCityPoints);
  const std::vector<ge::GeoAreaId> newarkHomes(kResidents, newark);
  const std::vector<ge::GeoAreaId> newYorkHomes(kResidents, newYork);
  const auto newarkPlan = buildPlan(&metro, newarkHomes);
  const auto newYorkPlan = buildPlan(&metro, newYorkHomes);
  const auto newarkPoint = metro.external.atmTerminals.front();
  std::unordered_set<Key> fillUnion;
  for (pl::entity::PersonId p = 1; p <= kResidents; ++p) {
    const auto fromNewark =
        newarkPlan.counterparties().withdrawalPointsFor(p, 0);
    PL_CHECK_EQ(fromNewark.size(), cash::kNearbyCount);
    PL_CHECK(fromNewark.span().front() == newarkPoint);
    for (const auto key : fromNewark.span().subspan(1)) {
      PL_CHECK(key != newarkPoint);
      fillUnion.insert(key);
    }

    // A5: the same person living in New York gets New York points only.
    const auto fromNewYork =
        newYorkPlan.counterparties().withdrawalPointsFor(p, 0);
    PL_CHECK_EQ(fromNewYork.size(), cash::kNearbyCount);
    PL_CHECK(std::ranges::find(fromNewYork.span(), newarkPoint) ==
             fromNewYork.span().end());
  }
  PL_CHECK_EQ(fillUnion.size(), kCityPoints);
  std::printf("  PASS: A2/A5 a straddling fill keeps the nearer point and "
              "spreads the rest over all %zu tied points\n",
              fillUnion.size());

  // A3: where every distance group fits inside the cut, the selection is
  // element for element the former list. (i) four points in four areas;
  // (ii) seven points whose groups end exactly at the cut for New York and
  // Newark residents; (iii) the directory-less two-point fallback; (iv) a
  // person with no home area, who gets the whole small pool in pool order.
  const std::vector<ge::GeoAreaId> spreadAreas{
      newYork, areaNamed("San Francisco"), areaNamed("Oakland"), newark};
  pl::entity::counterparty::Directory spread;
  std::vector<ge::GeoAreaId> spreadHomes;
  for (const auto area : spreadAreas) {
    place(spread, area, 1);
    spreadHomes.insert(spreadHomes.end(), 50, area);
  }
  const auto spreadPlan = buildPlan(&spread, spreadHomes);
  const auto &spreadAccess = spreadPlan.counterparties();
  for (pl::entity::PersonId p = 1; p <= spreadHomes.size(); ++p) {
    const auto home = spreadHomes[p - 1U];
    PL_CHECK(keysOf(spreadAccess.withdrawalPointsFor(p, 0)) ==
             indexTieBreak(spread.external.atmTerminals,
                           spread.external.atmTerminalAreas, home));
    PL_CHECK(keysOf(spreadAccess.depositPointsFor(p, 0)) ==
             indexTieBreak(spread.external.cashDepositories,
                           spread.external.cashDepositoryAreas, home));
    PL_CHECK(keysOf(spreadAccess.checkDepositPointsFor(p, 0)) ==
             indexTieBreak(spread.external.checkCapturePoints,
                           spread.external.checkCaptureAreas, home));
  }
  const auto homeless =
      static_cast<pl::entity::PersonId>(spreadHomes.size() + 1U);
  PL_CHECK(keysOf(spreadAccess.withdrawalPointsFor(homeless, 0)) ==
           spread.external.atmTerminals);

  pl::entity::counterparty::Directory aligned;
  place(aligned, newYork, 2);
  place(aligned, newark, 2);
  place(aligned, areaNamed("San Francisco"), 3);
  std::vector<ge::GeoAreaId> alignedHomes(100, newYork);
  alignedHomes.insert(alignedHomes.end(), 100, newark);
  const auto alignedPlan = buildPlan(&aligned, alignedHomes);
  for (pl::entity::PersonId p = 1; p <= alignedHomes.size(); ++p) {
    PL_CHECK(keysOf(alignedPlan.counterparties().withdrawalPointsFor(p, 0)) ==
             indexTieBreak(aligned.external.atmTerminals,
                           aligned.external.atmTerminalAreas,
                           alignedHomes[p - 1U]));
  }

  const auto barePlan = buildPlan(nullptr, cityHomes);
  const auto &bare = barePlan.counterparties();
  for (pl::entity::PersonId p = 1; p <= 50; ++p) {
    PL_CHECK(keysOf(bare.withdrawalPointsFor(p, 0)) ==
             (std::vector<Key>{cash::atmTerminal(1), cash::atmTerminal(2)}));
    PL_CHECK(keysOf(bare.depositPointsFor(p, 0)) ==
             (std::vector<Key>{cash::depository(1), cash::depository(2)}));
    PL_CHECK(keysOf(bare.checkDepositPointsFor(p, 0)) ==
             (std::vector<Key>{cash::checkCapture(1), cash::checkCapture(2)}));
  }
  std::printf("  PASS: A3 pools whose groups fit the cut select exactly the "
              "former list\n");

  // A4: a pure function of (area, person, rail): a second build and a
  // reversed query order give the same sets.
  const auto twinPlan = buildPlan(&city, cityHomes);
  for (auto p = static_cast<pl::entity::PersonId>(kResidents); p >= 1; --p) {
    PL_CHECK(keysOf(twinPlan.counterparties().withdrawalPointsFor(p, 0)) ==
             keysOf(cityAccess.withdrawalPointsFor(p, 0)));
    PL_CHECK(keysOf(twinPlan.counterparties().depositPointsFor(p, 0)) ==
             keysOf(cityAccess.depositPointsFor(p, 0)));
  }

  // A6: the depository and check pools have identical placement, so only the
  // per-rail domain keeps a person's two windows apart.
  std::size_t differ = 0;
  for (pl::entity::PersonId p = 1; p <= kResidents; ++p) {
    std::vector<std::uint64_t> deposit;
    std::vector<std::uint64_t> check;
    for (const auto key : keysOf(cityAccess.depositPointsFor(p, 0))) {
      deposit.push_back(key.number - cash::kDepositoryBaseSerial);
    }
    for (const auto key : keysOf(cityAccess.checkDepositPointsFor(p, 0))) {
      check.push_back(key.number - cash::kCheckCaptureBaseSerial);
    }
    differ += deposit == check ? 0U : 1U;
  }
  PL_CHECK(differ * 2U >= kResidents);
  std::printf("  PASS: A4/A6 sets are order-independent, and %zu of %zu "
              "persons have different depository and check windows\n",
              differ, kResidents);
}

// ---------------------------------------------------------------------------
// The scale gate. No ledger and no corpus: each person is a hash-based ATM
// user with the routine's userP of 0.88 and 42 withdrawals a year (the mean
// of U{1..6} a month), so rows are before the affordability screen and
// deaths. Homes are drawn per person, not per household.

inline constexpr std::uint64_t kUserCoinDomain = 0x4154'4D55'5345'5201ULL;
inline constexpr std::int64_t kYearStart = 1'704'067'200LL; // 2024-01-01

enum class Rail : std::size_t { atm = 0, depository = 1, check = 2 };
inline constexpr std::size_t kRailCount = 3;
inline constexpr std::array<const char *, kRailCount> kRailNames{
    "ATM", "depository", "check capture"};

struct RailShape {
  std::size_t pool = 0;
  std::size_t used = 0;
  // Big areas hold more than kNearbyCount own points.
  std::size_t bigAreas = 0;
  // G1: used points over the pool.
  double coverage = 0.0;
  // G2: the worst big area's busiest own point, as a multiple of its fair
  // share of that area's residents' rows.
  double within = 0.0;
  // G3: the worst big area's share of own points its residents use.
  double ownCoverage = 1.0;
  // G4: busiest point over the mean load.
  double maxOverMean = 0.0;
  // G5: the largest (row share) / (area resident share / area point count).
  double residentRatio = 0.0;
  std::string withinCity;
  double rowsP50 = 0.0;
  double rowsMax = 0.0;
  std::size_t accountsP50 = 0;
  std::size_t accountsMax = 0;
};

[[nodiscard]] std::span<const Key> pointsOf(const cash::LocalPoints &local) {
  return local.span();
}
[[nodiscard]] std::span<const Key> pointsOf(const std::vector<Key> &local) {
  return local;
}

template <class SetFor, class Pick>
[[nodiscard]] RailShape
measureRail(std::span<const Key> points, std::span<const ge::GeoAreaId> at,
            std::span<const ge::GeoAreaId> homes, int eventsPerYear,
            SetFor &&setFor, Pick &&pick) {
  const auto &geo = pl::synth::geo::geography();
  std::unordered_map<Key, std::size_t> ixOf;
  for (std::size_t i = 0; i < points.size(); ++i) {
    ixOf.emplace(points[i], i);
  }
  std::unordered_map<ge::GeoAreaId, std::size_t> residents;
  std::unordered_map<ge::GeoAreaId, std::size_t> pointsIn;
  std::unordered_map<ge::GeoAreaId, double> areaRows;
  for (const auto home : homes) {
    ++residents[home];
  }
  for (const auto area : at) {
    ++pointsIn[area];
  }

  std::vector<double> load(points.size(), 0.0);
  std::vector<double> ownRows(points.size(), 0.0);
  std::vector<std::size_t> accounts(points.size(), 0);
  std::vector<pl::entity::PersonId> lastSeen(points.size(),
                                             pl::entity::invalidPerson);
  const std::int64_t step = 365LL * 86'400LL / eventsPerYear;
  for (std::size_t ix = 0; ix < homes.size(); ++ix) {
    const auto person = static_cast<pl::entity::PersonId>(ix + 1U);
    if (cash::detail::splitmix(person ^ kUserCoinDomain) % 100U >= 88U) {
      continue;
    }
    const auto account = pl::entity::makeKey(
        pl::entity::Role::account, pl::entity::Bank::internal, person);
    for (int w = 0; w < eventsPerYear; ++w) {
      const std::int64_t ts = kYearStart + w * step +
                              static_cast<std::int64_t>(person % 997U) * 61;
      const auto &local = setFor(person, ts);
      const auto chosen = pick(pointsOf(local), account, ts);
      const auto i = ixOf.at(chosen);
      load[i] += 1.0;
      areaRows[homes[ix]] += 1.0;
      if (at[i] == homes[ix]) {
        ownRows[i] += 1.0;
      }
      if (lastSeen[i] != person) {
        lastSeen[i] = person;
        ++accounts[i];
      }
    }
  }

  RailShape out;
  out.pool = points.size();
  double total = 0.0;
  for (const auto rows : load) {
    total += rows;
    out.used += rows > 0.0 ? 1U : 0U;
  }
  out.coverage = static_cast<double>(out.used) / static_cast<double>(out.pool);
  const double mean = total / static_cast<double>(out.pool);
  const double busiest = *std::ranges::max_element(load);
  out.maxOverMean = busiest / mean;

  for (std::size_t i = 0; i < points.size(); ++i) {
    const double fair = static_cast<double>(residents[at[i]]) /
                        static_cast<double>(homes.size()) /
                        static_cast<double>(pointsIn[at[i]]);
    if (fair > 0.0) {
      out.residentRatio = std::max(out.residentRatio, load[i] / total / fair);
    }
  }

  for (const auto &[area, count] : pointsIn) {
    if (count <= cash::kNearbyCount || areaRows[area] <= 0.0) {
      continue;
    }
    ++out.bigAreas;
    double busiestOwn = 0.0;
    std::size_t usedOwn = 0;
    for (std::size_t i = 0; i < points.size(); ++i) {
      if (at[i] != area) {
        continue;
      }
      busiestOwn = std::max(busiestOwn, ownRows[i]);
      usedOwn += ownRows[i] > 0.0 ? 1U : 0U;
    }
    const double within =
        busiestOwn / areaRows[area] * static_cast<double>(count);
    if (within > out.within) {
      out.within = within;
      out.withinCity = geo.at(area).city;
    }
    out.ownCoverage =
        std::min(out.ownCoverage,
                 static_cast<double>(usedOwn) / static_cast<double>(count));
  }

  std::vector<double> sortedRows(load);
  std::ranges::sort(sortedRows);
  std::vector<std::size_t> sortedAccounts(accounts);
  std::ranges::sort(sortedAccounts);
  out.rowsP50 = sortedRows[sortedRows.size() / 2U];
  out.rowsMax = sortedRows.back();
  out.accountsP50 = sortedAccounts[sortedAccounts.size() / 2U];
  out.accountsMax = sortedAccounts.back();
  return out;
}

void printShape(const char *arm, const char *rail, int population,
                const RailShape &shape) {
  std::printf("    %-9s %-13s pop %7d: used %zu/%zu (G1 %.3f) | G2 within "
              "%.2f (%s, %zu big areas) | G3 own coverage %.3f | G4 "
              "max/mean %.2f | G5 resident ratio %.2f\n",
              arm, rail, population, shape.used, shape.pool, shape.coverage,
              shape.within,
              shape.withinCity.empty() ? "-" : shape.withinCity.c_str(),
              shape.bigAreas, shape.ownCoverage, shape.maxOverMean,
              shape.residentRatio);
}

struct ScaleLeg {
  int population = 0;
  std::array<bool, kRailCount> bounded{};
};

// Armed bands, set between the armed and index-tie-break measurements on
// this code (the audit amendment atm-spread-2026-09 lists both arms). G5 is
// banded for ATMs only: at 25 depositories per 100,000 people most areas hold
// one or two, whose fill from neighbouring areas sets G5 in both arms (5.70
// armed against 6.47 at pop 500,000), so a band there could not fail.
inline constexpr double kMinCoverage = 0.99;
inline constexpr double kMaxWithin = 1.5;
inline constexpr double kMaxOverMeanAtm = 2.5;
inline constexpr double kMaxOverMeanDepository = 4.0;
inline constexpr double kMaxResidentRatioAtm = 3.5;
// The research target for the busiest terminal, 20,000 to 50,000 a year.
inline constexpr double kMaxBusiestAtmRows = 50'000.0;
// Preconditions: the reference arm must express the defect at a bounded leg.
inline constexpr double kMinReferenceWithinAtm = 5.0;
inline constexpr double kMinReferenceWithinDepository = 3.0;

void runScaleLeg(const ScaleLeg &leg) {
  const auto &geo = pl::synth::geo::geography();
  const pl::synth::geo::ResidenceSampler residence{geo};
  const auto mix = pl::synth::pii::LocaleMix::usBankDefault();
  auto homeRng = pl::random::Rng::fromSeed(0x5EED'0001ULL);
  std::vector<ge::GeoAreaId> homes(static_cast<std::size_t>(leg.population));
  for (auto &home : homes) {
    home = residence.sample(homeRng,
                            pl::synth::pii::sampleCountry(homeRng, mix));
  }
  auto directoryRng = pl::random::Rng::fromSeed(0xC0FFEEULL);
  const auto directory = pl::synth::counterparties::make(
      directoryRng, leg.population, {}, homes);
  const auto plan = buildPlan(&directory, homes);
  const auto &access = plan.counterparties();

  // Business cash takings resolve their depository through the revenue book,
  // a second copy of the area lookup; wired exactly as passes.cpp wires it.
  pl::activity::income::RevenueCounterparties revenue;
  revenue.directory = &directory;
  revenue.cashDepositPoints = access.cashDepositPoints;
  revenue.nearbyCashDepositPoints = &access.nearbyCashDepositPoints;
  revenue.homeAreas = access.homeAreas;
  revenue.relocation = access.relocation;

  // A4 at scale: a second build agrees with the first on every rail, and the
  // revenue lookup agrees with the household one.
  const auto twin = buildPlan(&directory, homes);
  for (std::size_t ix = homes.size(); ix-- > 0;) {
    const auto p = static_cast<pl::entity::PersonId>(ix + 1U);
    PL_CHECK(keysOf(revenue.cashDepositoriesFor(p, kYearStart)) ==
             keysOf(access.depositPointsFor(p, kYearStart)));
    if (ix % 97U == 0U) {
      PL_CHECK(keysOf(twin.counterparties().withdrawalPointsFor(p, 0)) ==
               keysOf(access.withdrawalPointsFor(p, 0)));
      PL_CHECK(keysOf(twin.counterparties().checkDepositPointsFor(p, 0)) ==
               keysOf(access.checkDepositPointsFor(p, 0)));
    }
  }

  const std::array<std::span<const Key>, kRailCount> pools{
      directory.external.atmTerminals, directory.external.cashDepositories,
      directory.external.checkCapturePoints};
  const std::array<std::span<const ge::GeoAreaId>, kRailCount> areas{
      directory.external.atmTerminalAreas,
      directory.external.cashDepositoryAreas,
      directory.external.checkCaptureAreas};
  std::array<std::unordered_map<ge::GeoAreaId, std::vector<Key>>, kRailCount>
      reference;
  for (std::size_t r = 0; r < kRailCount; ++r) {
    for (const auto home : homes) {
      if (!reference[r].contains(home)) {
        reference[r].emplace(home, indexTieBreak(pools[r], areas[r], home));
      }
    }
  }

  const auto terminal = [](std::span<const Key> local, Key account,
                           std::int64_t ts) {
    return cash::terminalFor(local, account, ts);
  };
  // Household deposits pick with deposits.cpp's stableExternalPoint and
  // business takings with depositoryFor: both are a stable per-account hash
  // modulo the local set, so depositoryFor stands in for both.
  const auto stable = [](std::span<const Key> local, Key account,
                         std::int64_t) {
    return cash::depositoryFor(local, account);
  };

  for (std::size_t r = 0; r < kRailCount; ++r) {
    const auto rail = static_cast<Rail>(r);
    const int events = rail == Rail::atm ? 42 : 12;
    const auto armedSet = [&](pl::entity::PersonId p, std::int64_t ts) {
      switch (rail) {
      case Rail::atm:
        return access.withdrawalPointsFor(p, ts);
      case Rail::depository:
        return revenue.cashDepositoriesFor(p, ts);
      case Rail::check:
        break;
      }
      return access.checkDepositPointsFor(p, ts);
    };
    const auto referenceSet = [&](pl::entity::PersonId p,
                                  std::int64_t) -> const std::vector<Key> & {
      return reference[r].at(homes[p - 1U]);
    };
    const auto armed =
        rail == Rail::atm
            ? measureRail(pools[r], areas[r], homes, events, armedSet,
                          terminal)
            : measureRail(pools[r], areas[r], homes, events, armedSet, stable);
    const auto former =
        rail == Rail::atm
            ? measureRail(pools[r], areas[r], homes, events, referenceSet,
                          terminal)
            : measureRail(pools[r], areas[r], homes, events, referenceSet,
                          stable);
    printShape("armed", kRailNames[r], leg.population, armed);
    printShape("reference", kRailNames[r], leg.population, former);

    if (rail == Rail::atm) {
      // Anchors, printed and not banded. Payments Study CY2024: 3.4B
      // withdrawals over 451,500 (Euromonitor 2022) to 540,000 (ATMIA)
      // terminals; Cardtronics 2019: 757 a month; Bank of America 2024:
      // about 4,600 clients per ATM.
      std::printf("    ATM rows a year per terminal (before the screen): p50 "
                  "%.0f, max %.0f (reference max %.0f); distinct accounts "
                  "p50 %zu, max %zu (reference max %zu)\n",
                  armed.rowsP50, armed.rowsMax, former.rowsMax,
                  armed.accountsP50, armed.accountsMax, former.accountsMax);
      std::printf("    anchors: US mean 6,296 to 7,530 withdrawals a year "
                  "(3.4B / 540,000 to 451,500), Cardtronics 9,084, Bank of "
                  "America about 4,600 clients per ATM; research target for "
                  "the busiest terminal 20,000 to 50,000\n");
    }

    if (!leg.bounded[r]) {
      continue;
    }
    const bool atm = rail == Rail::atm;
    // Precondition: the leg can express the defect.
    PL_CHECK(former.bigAreas >= 1U);
    PL_CHECK(former.within >=
             (atm ? kMinReferenceWithinAtm : kMinReferenceWithinDepository));
    // G1 to G4 on every bounded rail, G5 and G6 on ATMs.
    PL_CHECK(armed.coverage >= kMinCoverage);
    PL_CHECK(armed.within <= kMaxWithin);
    PL_CHECK(armed.ownCoverage >= 1.0);
    PL_CHECK(armed.maxOverMean <=
             (atm ? kMaxOverMeanAtm : kMaxOverMeanDepository));
    if (atm) {
      PL_CHECK(armed.residentRatio <= kMaxResidentRatioAtm);
      PL_CHECK(armed.rowsMax <= kMaxBusiestAtmRows);
    }
    std::printf("  PASS: %s at pop %d spreads over every point (%s)\n",
                kRailNames[r], leg.population, atm ? "G1-G6" : "G1-G4");
  }
}

void testCashPointConcentrationAtScale() {
  runScaleLeg({.population = 10'000, .bounded = {false, false, false}});
  runScaleLeg({.population = 200'000, .bounded = {true, false, false}});
  runScaleLeg({.population = 500'000, .bounded = {true, true, true}});
}

} // namespace nearby

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
  nearby::testNearbySetsSpreadTiedPoints();
  nearby::testCashPointConcentrationAtScale();
  std::printf("All cash boundary regression tests passed.\n");
  return 0;
}
