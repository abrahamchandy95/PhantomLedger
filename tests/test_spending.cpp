#include "phantomledger/activity/spending/liquidity/factor.hpp"
#include "phantomledger/activity/spending/liquidity/multiplier.hpp"
#include "phantomledger/activity/spending/liquidity/snapshot.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/simulator/session.hpp"
#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/activity/spending/spenders/targets.hpp"
#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/routines/spending_session.hpp"

#include "gate_world.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace PhantomLedger;
namespace routing = PhantomLedger::activity::spending::routing;
namespace liquidity = PhantomLedger::activity::spending::liquidity;
namespace spenders = PhantomLedger::activity::spending::spenders;

namespace {

constexpr double kEps = 1e-9;

[[nodiscard]] inline bool nearly(double lhs, double rhs, double tol = kEps) {
  return std::abs(lhs - rhs) <= tol;
}

// -------------------------- Channel CDF --------------------------

void testChannelCdfShape() {
  // Equal weights for the three core channels, no unknown outflow:
  // the CDF should be (1/3, 2/3, 1, 1).
  const auto cdf = routing::buildChannelCdf(1.0, 1.0, 1.0, 0.0);
  PL_CHECK(nearly(cdf[0], 1.0 / 3.0));
  PL_CHECK(nearly(cdf[1], 2.0 / 3.0));
  PL_CHECK(nearly(cdf[2], 1.0));
  PL_CHECK(nearly(cdf[3], 1.0));
  std::println("  PASS: channel CDF — equal weights");
}

void testChannelCdfWithUnknown() {
  // 20% unknown takes a fixed slice; the other 80% is split per the
  // (merchant, bills, p2p) ratio (here 6:3:1).
  const auto cdf = routing::buildChannelCdf(0.6, 0.3, 0.1, 0.20);
  // s0 = 0.8 * 0.6 = 0.48
  PL_CHECK(nearly(cdf[0], 0.48));
  // cdf[1] = 0.48 + 0.8*0.3 = 0.72
  PL_CHECK(nearly(cdf[1], 0.72));
  // cdf[2] = 0.72 + 0.8*0.1 = 0.80
  PL_CHECK(nearly(cdf[2], 0.80));
  // cdf[3] = 0.80 + 0.20 = 1.0
  PL_CHECK(nearly(cdf[3], 1.0));
  std::println("  PASS: channel CDF — unknown carved out cleanly");
}

void testChannelCdfFallsBackOnZeroCore() {
  // Every core weight zero: the implementation falls back to uniform
  // 1/3 across the three core channels and respects the unknown share.
  const auto cdf = routing::buildChannelCdf(0.0, 0.0, 0.0, 0.10);
  PL_CHECK(nearly(cdf[0], 0.30));
  PL_CHECK(nearly(cdf[1], 0.60));
  PL_CHECK(nearly(cdf[2], 0.90));
  PL_CHECK(nearly(cdf[3], 1.0));
  std::println("  PASS: channel CDF — zero core uses uniform fallback");
}

void testChannelCdfClampsUnknown() {
  // unknownOutflowP is clamped to [0, 1].
  const auto neg = routing::buildChannelCdf(1.0, 1.0, 1.0, -0.5);
  PL_CHECK(nearly(neg[3], 1.0));
  PL_CHECK(nearly(neg[0], 1.0 / 3.0));

  const auto over = routing::buildChannelCdf(1.0, 1.0, 1.0, 1.7);
  // All mass goes to unknown.
  PL_CHECK(nearly(over[0], 0.0));
  PL_CHECK(nearly(over[1], 0.0));
  PL_CHECK(nearly(over[2], 0.0));
  PL_CHECK(nearly(over[3], 1.0));
  std::println("  PASS: channel CDF — unknownP clamped to [0, 1]");
}

void testPickSlot() {
  routing::ChannelCdf cdf{};
  cdf[0] = 0.25;
  cdf[1] = 0.50;
  cdf[2] = 0.75;
  cdf[3] = 1.00;

  PL_CHECK(routing::pickSlot(cdf, 0.0) == routing::Slot::merchant);
  PL_CHECK(routing::pickSlot(cdf, 0.10) == routing::Slot::merchant);
  PL_CHECK(routing::pickSlot(cdf, 0.30) == routing::Slot::bill);
  PL_CHECK(routing::pickSlot(cdf, 0.55) == routing::Slot::p2p);
  PL_CHECK(routing::pickSlot(cdf, 0.80) == routing::Slot::externalUnknown);
  PL_CHECK(routing::pickSlot(cdf, 0.999) == routing::Slot::externalUnknown);
  std::println("  PASS: pickSlot ladder");
}

// ------------------------ Liquidity factor ------------------------

void testCountFactorMonotonic() {
  // Shape claim from the implementation: clamp to [0, 1.25], soft
  // shape below 1.0, square. So the function is monotone non-decreasing
  // in liquidity for liquidity in [0, 1.25].
  double prev = liquidity::countFactor(0.0);
  for (double x = 0.05; x <= 1.25; x += 0.05) {
    double cur = liquidity::countFactor(x);
    PL_CHECK(cur + 1e-12 >= prev);
    prev = cur;
  }
  std::println("  PASS: countFactor monotone non-decreasing");
}

void testCountFactorBoundaries() {
  // At liquidity = 0:   softened = 0.50, factor = 0.25.
  PL_CHECK(nearly(liquidity::countFactor(0.0), 0.25));
  // At liquidity = 1:   softened = 1.0,  factor = 1.0.
  PL_CHECK(nearly(liquidity::countFactor(1.0), 1.0));
  // At liquidity > 1.25: clamped to 1.25, factor = 1.5625.
  PL_CHECK(nearly(liquidity::countFactor(1.25), 1.5625));
  PL_CHECK(nearly(liquidity::countFactor(5.0), 1.5625));
  // Negative liquidity is clamped to 0, then softened+squared = 0.25.
  PL_CHECK(nearly(liquidity::countFactor(-1.0), 0.25));
  std::println("  PASS: countFactor boundary values");
}

// ----------------------- Liquidity multiplier ---------------------

// Builds a Snapshot for the multiplier under test. The `availableToSpend`
// field represents the spender's full spending capacity (cash +
// overdraft + LOC + courtesy buffer), mirroring Python's
// ClearingHouse.available_to_spend. The tests below feed values that
// happen to coincide with cash-only figures because the suppression
// curve's shape is what's under test, not the semantic of the input.
[[nodiscard]] liquidity::Snapshot makeSnap(std::uint32_t daysSincePayday,
                                           double sensitivity,
                                           double availableToSpend,
                                           double baselineCash, double burden) {
  return liquidity::Snapshot{
      .daysSincePayday = daysSincePayday,
      .paycheckSensitivity = sensitivity,
      .availableToSpend = availableToSpend,
      .baselineCash = baselineCash,
      .fixedMonthlyBurden = burden,
  };
}

void testMultiplierDisabled() {
  liquidity::Throttle disabled{};
  disabled.enabled = false;

  const auto snap = makeSnap(/*daysSincePayday=*/365, /*sensitivity=*/0.9,
                             /*availableToSpend=*/0.0, /*baselineCash=*/1000.0,
                             /*burden=*/2000.0);
  PL_CHECK(nearly(liquidity::multiplier(disabled, snap), 1.0));
  std::println("  PASS: liquidity multiplier — disabled returns 1.0");
}

void testMultiplierStressRegion() {
  const liquidity::Throttle cfg{};

  const auto fresh = makeSnap(/*daysSincePayday=*/0, /*sensitivity=*/0.5,
                              /*availableToSpend=*/1000.0,
                              /*baselineCash=*/1000.0,
                              /*burden=*/0.0);

  const auto stressed =
      makeSnap(/*daysSincePayday=*/30, /*sensitivity=*/0.5,
               /*availableToSpend=*/1000.0, /*baselineCash=*/1000.0,
               /*burden=*/0.0);

  const double freshMult = liquidity::multiplier(cfg, fresh);
  const double stressedMult = liquidity::multiplier(cfg, stressed);

  PL_CHECK(stressedMult < freshMult);

  PL_CHECK(stressedMult >= cfg.absoluteFloor - 1e-12);
  PL_CHECK(freshMult <= liquidity::kCeiling + 1e-12);

  std::println("  PASS: liquidity multiplier — stress region < fresh region "
               "({:.3f} < {:.3f})",
               stressedMult, freshMult);
}

void testMultiplierBurdenPenalty() {
  const liquidity::Throttle cfg{};

  const auto noBurden =
      makeSnap(/*daysSincePayday=*/4, /*sensitivity=*/0.3,
               /*availableToSpend=*/500.0, /*baselineCash=*/500.0,
               /*burden=*/0.0);

  const auto highBurden =
      makeSnap(/*daysSincePayday=*/4, /*sensitivity=*/0.3,
               /*availableToSpend=*/500.0, /*baselineCash=*/500.0,
               /*burden=*/1000.0);

  PL_CHECK(liquidity::multiplier(cfg, highBurden) <
           liquidity::multiplier(cfg, noBurden));
  std::println("  PASS: liquidity multiplier — fixed burden penalises");
}

// ----------------------- Spenders helpers -------------------------

void testTotalTargetTxns() {
  // Spec: txnsPerMonth * activeSpenders * (days / 30).
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 30), 6000.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 90), 18000.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(0.0, 100, 30), 0.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 0, 30), 0.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 0), 0.0));
  std::println("  PASS: totalTargetTxns scaling");
}

void testCollectiveDayOversubscriptionIsFiltered() {
  namespace simulator = activity::spending::simulator;

  const auto card =
      entity::makeKey(entity::Role::card, entity::Bank::internal, 90'001);
  const auto merchant =
      entity::makeKey(entity::Role::business, entity::Bank::internal, 90'002);

  clearing::Ledger ledger;
  ledger.initialize(2);
  ledger.addAccount(card, 0);
  ledger.addAccount(merchant, 1);
  ledger.setOverdraftOnly(0, 100.0);
  ledger.createHub(1);

  transactions::Transaction later{};
  later.source = card;
  later.target = merchant;
  later.amount = 60.0;
  later.timestamp = 2;
  later.session.channel = channels::tag(channels::Legit::cardPurchase);

  auto earlier = later;
  earlier.timestamp = 1;

  // Deliberately supply generation order opposite to event-time order.
  std::vector<transactions::Transaction> dayTransactions{later, earlier};
  std::vector<clearing::Ledger::Posting> dayPostings{
      {
          .srcIdx = 0,
          .dstIdx = 1,
          .amount = later.amount,
          .channel = later.session.channel,
          .timestamp = later.timestamp,
      },
      {
          .srcIdx = 0,
          .dstIdx = 1,
          .amount = earlier.amount,
          .channel = earlier.session.channel,
          .timestamp = earlier.timestamp,
      },
  };
  std::vector<transactions::Transaction> accepted;

  // Both rows pass an independent day-start check against $100, but only the
  // event-time first can settle when the real postings are applied
  // sequentially. The returned vector is exactly the state slice DayDriver
  // subsequently hands to CardCycleDriver, so the declined later purchase
  // cannot be billed.
  simulator::detail::appendAcceptedDayPostings(&ledger, accepted,
                                               dayTransactions, dayPostings);

  PL_CHECK_EQ(accepted.size(), 1U);
  PL_CHECK_EQ(accepted.front().timestamp, earlier.timestamp);
  PL_CHECK(nearly(ledger.cash(0), -60.0));
  PL_CHECK(dayTransactions.empty());
  PL_CHECK(dayPostings.empty());
  std::println("  PASS: collectively oversubscribed same-day card purchase is "
               "filtered before card billing");
}

void testDayPostingCarriesTransactionTimestamp() {
  namespace simulator = activity::spending::simulator;

  transactions::Transaction txn{};
  txn.timestamp = 1'709'294'400;
  std::vector<transactions::Transaction> dayTransactions{txn};
  std::vector<clearing::Ledger::Posting> dayPostings{{
      .timestamp = 0,
  }};
  std::vector<transactions::Transaction> accepted;

  bool rejectedMismatch = false;
  try {
    simulator::detail::appendAcceptedDayPostings(nullptr, accepted,
                                                 dayTransactions, dayPostings);
  } catch (const std::logic_error &) {
    rejectedMismatch = true;
  }

  PL_CHECK(rejectedMismatch);
  PL_CHECK(accepted.empty());
  std::println(
      "  PASS: day settlement rejects an epoch-zero posting timestamp");
}

void testCreditCardUtilizationIsNotCheckingOverdraft() {
  const auto card =
      entity::makeKey(entity::Role::card, entity::Bank::internal, 91'001);
  const auto deposit =
      entity::makeKey(entity::Role::account, entity::Bank::internal, 91'002);
  constexpr double kFee = 35.0;
  constexpr double kPurchase = 20.0;
  const auto purchaseChannel = channels::tag(channels::Legit::cardPurchase);

  clearing::Ledger ledger;
  ledger.initialize(2);
  ledger.addAccount(card, 0);
  ledger.addAccount(deposit, 1);
  ledger.setBankTier(0, clearing::BankTier::standardFee, kFee);
  ledger.setBankTier(1, clearing::BankTier::standardFee, kFee);
  ledger.setOverdraftOnly(0, 100.0);
  ledger.setProtection(1, clearing::ProtectionType::courtesy, 100.0);

  std::vector<clearing::LiquidityEvent> fees;
  ledger.setLiquiditySink(
      [&](const clearing::LiquidityEvent &event) { fees.push_back(event); });

  PL_CHECK(ledger
               .transferAt({
                   .srcIdx = 0,
                   .dstIdx = clearing::Ledger::invalid,
                   .amount = kPurchase,
                   .channel = purchaseChannel,
                   .timestamp = 100,
               })
               .accepted());
  PL_CHECK_EQ(ledger.cash(0), -kPurchase);
  PL_CHECK(fees.empty());

  PL_CHECK(ledger
               .transferAt({
                   .srcIdx = 1,
                   .dstIdx = clearing::Ledger::invalid,
                   .amount = kPurchase,
                   .channel = purchaseChannel,
                   .timestamp = 100,
               })
               .accepted());
  PL_CHECK_EQ(ledger.cash(1), -(kPurchase + kFee));
  PL_CHECK_EQ(fees.size(), 1U);
  PL_CHECK(fees.front().payerKey == deposit);
  PL_CHECK_EQ(fees.front().amount, kFee);
  PL_CHECK(fees.front().channel ==
           channels::tag(channels::Liquidity::overdraftFee));

  std::println("  PASS: card utilization avoids checking overdraft fees while "
               "deposit courtesy fees remain intact");
}

// -------------- Session window-invariance matrix (step 1) ----------------
//
// A generation-window boundary is an eviction boundary, not a semantic
// boundary: advancing the persistent spending Session through any partition
// of the run window must produce the byte-identical corpus. Each leg below
// is built from a fresh, identically seeded GateWorld (gate_world.hpp);
// Session and Market are stateful and non-copyable, so reuse across legs
// would be invalid. The legs differ ONLY in how Session::advance() slices
// the run window. All comparisons are exact; no approximate equality.
//
// The leg deliberately runs without products, infra routing (Factory has
// no Router) and with an empty base-transaction stream: all are constant
// across legs, so they cannot mask a window-size dependence, and they
// keep the gate focused on the Session itself.

namespace pl = ::PhantomLedger;
namespace routineSpending = pl::transfers::legit::routines::spending;
namespace plSim = pl::activity::spending::simulator;

struct SessionLeg {
  std::string digest;
  std::vector<pl::transactions::Transaction> rows;
  std::uint64_t cardEvents = 0;
};

// One complete leg: fresh world, fresh session, one window partition.
// monthsPerWindow == 0 selects the full-range leg (a single advance()).
[[nodiscard]] SessionLeg runSessionLeg(const pl::synth::pii::PoolSet &poolSet,
                                       std::uint64_t seed,
                                       pl::time::Window window,
                                       int monthsPerWindow) {
  pltest::WorldSpec spec;
  spec.seed = seed;
  spec.window = window;
  spec.population = 300;
  // Production-default fraud profile; no products, no infra, no base
  // streams (see the section comment above).
  spec.withProducts = false;
  spec.withInfra = false;
  spec.withInfraRouting = false;
  spec.withIncome = false;
  spec.withBaseRoutines = false;

  pltest::GateWorld world(poolSet, spec);

  routineSpending::SessionInputs inputs;
  inputs.threadCount = 1;
  inputs.cardLifecycle = world.cardCfg;

  const auto bundle = routineSpending::SessionBundle::make(
      seed, world.rng, *world.txf, world.market, world.obligations,
      world.screenBook, std::move(inputs));

  auto &session = bundle->session();

  SessionLeg leg;
  pl::time::TimePoint expectedStart = window.start;
  int finalizedDays = 0;

  const auto consume = [&](plSim::Batch output) {
    if (output.finalized.days > 0) {
      PL_CHECK(output.finalized.start == expectedStart);
      expectedStart = output.finalized.endExcl();
      finalizedDays += output.finalized.days;
    }
    leg.rows.insert(leg.rows.end(),
                    std::make_move_iterator(output.txns.begin()),
                    std::make_move_iterator(output.txns.end()));
  };

  if (monthsPerWindow == 0) {
    consume(session.advance(window));
  } else {
    const auto schedule = pl::pipeline::chunk::Schedule::partition(
        window, pl::pipeline::chunk::Strategy{
                    .monthsPerChunk = monthsPerWindow,
                    .lookaheadDays = 6,
                });
    for (const auto &span : schedule) {
      consume(session.advance(span.activeWindow));
    }
  }
  consume(session.finish());

  // Finalized coverage must tile the run window exactly.
  PL_CHECK(expectedStart == window.endExcl());
  PL_CHECK(finalizedDays == window.days);

  leg.cardEvents = session.cardEventCount();

  const auto wrap = pl::pipeline::chunk::Schedule::unpartitioned(window);
  pl::exporter::sinks::Golden golden;
  golden.beginSpan(*wrap.begin());
  golden.append(std::span<const pl::transactions::Transaction>(
      leg.rows.data(), leg.rows.size()));
  golden.endSpan(*wrap.begin());
  golden.finish();
  leg.digest = golden.digest();

  return leg;
}

void reportFirstRowDifference(
    const std::vector<pl::transactions::Transaction> &a,
    const std::vector<pl::transactions::Transaction> &b) {
  const auto n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (pl::transactions::detail::auditKey(a[i]) !=
        pl::transactions::detail::auditKey(b[i])) {
      const auto &lhs = a[i];
      const auto &rhs = b[i];

      std::println(stderr,
                   "  first differing row {}:\n"
                   "    full-range: ts={} src={} dst={} amt={:.10g} ch={}\n"
                   "    windowed:   ts={} src={} dst={} amt={:.10g} ch={}",
                   i, lhs.timestamp, lhs.source.number, lhs.target.number,
                   lhs.amount, static_cast<unsigned>(lhs.session.channel.value),
                   rhs.timestamp, rhs.source.number, rhs.target.number,
                   rhs.amount,
                   static_cast<unsigned>(rhs.session.channel.value));
      return;
    }
  }

  std::println(stderr,
               "  no differing row in the common prefix; row counts {} vs {}",
               a.size(), b.size());
}

void checkLegMatchesReference(const char *label, const SessionLeg &reference,
                              const SessionLeg &leg) {
  const bool equal = leg.digest == reference.digest &&
                     leg.rows.size() == reference.rows.size() &&
                     leg.cardEvents == reference.cardEvents;
  if (!equal) {
    std::println(stderr,
                 "[session-invariance] {} diverges from full range:\n"
                 "  rows: {} vs {}\n"
                 "  cardEvents: {} vs {}\n"
                 "  digest: {} vs {}",
                 label, leg.rows.size(), reference.rows.size(), leg.cardEvents,
                 reference.cardEvents, leg.digest, reference.digest);
    reportFirstRowDifference(reference.rows, leg.rows);
    PL_CHECK(equal);
  }
  std::println("  PASS: session invariance — {} matches full range", label);
}

void testSessionWindowInvariance() {
  constexpr std::uint64_t seed = 20260716;

  pl::time::Window window;
  window.start = pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2; // several 12-month windows; many 1-month windows

  const auto poolSet = pltest::buildPoolSet(seed);

  const auto reference = runSessionLeg(poolSet, seed, window, 0);

  // The gate is vacuous if the leg produced nothing or the card-lifecycle
  // path (the strongest cross-window state) never engaged.
  PL_CHECK(!reference.rows.empty());
  PL_CHECK(reference.cardEvents > 0);
  PL_CHECK(reference.digest.size() == 64);

  const struct {
    int months;
    const char *label;
  } legs[] = {
      {12, "12-month windows"},
      {6, "6-month windows"},
      {3, "3-month windows"},
      {1, "1-month windows"},
  };

  for (const auto &spec : legs) {
    const auto leg = runSessionLeg(poolSet, seed, window, spec.months);
    checkLegMatchesReference(spec.label, reference, leg);
  }
}

} // namespace

int main() {
  std::println("=== Spending Pipeline Tests ===");
  testChannelCdfShape();
  testChannelCdfWithUnknown();
  testChannelCdfFallsBackOnZeroCore();
  testChannelCdfClampsUnknown();
  testPickSlot();
  testCountFactorMonotonic();
  testCountFactorBoundaries();
  testMultiplierDisabled();
  testMultiplierStressRegion();
  testMultiplierBurdenPenalty();
  testTotalTargetTxns();
  testCollectiveDayOversubscriptionIsFiltered();
  testDayPostingCarriesTransactionTimestamp();
  testCreditCardUtilizationIsNotCheckingOverdraft();

  std::println("=== Spending Session Window Invariance ===");
  testSessionWindowInvariance();

  std::println("All spending pipeline tests passed.\n");
  return 0;
}
