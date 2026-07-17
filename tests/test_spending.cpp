#include "phantomledger/activity/spending/liquidity/factor.hpp"
#include "phantomledger/activity/spending/liquidity/multiplier.hpp"
#include "phantomledger/activity/spending/liquidity/snapshot.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/simulator/session.hpp"
#include "phantomledger/activity/spending/spenders/targets.hpp"
#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/routines/spending.hpp"
#include "phantomledger/transfers/legit/routines/spending_session.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
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
  std::printf("  PASS: channel CDF — equal weights\n");
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
  std::printf("  PASS: channel CDF — unknown carved out cleanly\n");
}

void testChannelCdfFallsBackOnZeroCore() {
  // Every core weight zero: the implementation falls back to uniform
  // 1/3 across the three core channels and respects the unknown share.
  const auto cdf = routing::buildChannelCdf(0.0, 0.0, 0.0, 0.10);
  PL_CHECK(nearly(cdf[0], 0.30));
  PL_CHECK(nearly(cdf[1], 0.60));
  PL_CHECK(nearly(cdf[2], 0.90));
  PL_CHECK(nearly(cdf[3], 1.0));
  std::printf("  PASS: channel CDF — zero core uses uniform fallback\n");
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
  std::printf("  PASS: channel CDF — unknownP clamped to [0, 1]\n");
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
  std::printf("  PASS: pickSlot ladder\n");
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
  std::printf("  PASS: countFactor monotone non-decreasing\n");
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
  std::printf("  PASS: countFactor boundary values\n");
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
  std::printf("  PASS: liquidity multiplier — disabled returns 1.0\n");
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

  std::printf("  PASS: liquidity multiplier — stress region < fresh region "
              "(%.3f < %.3f)\n",
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
  std::printf("  PASS: liquidity multiplier — fixed burden penalises\n");
}

// ----------------------- Spenders helpers -------------------------

void testTotalTargetTxns() {
  // Spec: txnsPerMonth * activeSpenders * (days / 30).
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 30), 6000.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 90), 18000.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(0.0, 100, 30), 0.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 0, 30), 0.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 0), 0.0));
  std::printf("  PASS: totalTargetTxns scaling\n");
}

// -------------- Session window-invariance matrix (step 1) ----------------
//
// A generation-window boundary is an eviction boundary, not a semantic
// boundary: advancing the persistent spending Session through any partition
// of the run window must produce the byte-identical corpus. Each leg below
// is built from a fresh, identically seeded world (Session and Market are
// stateful and non-copyable, so reuse across legs would be invalid). The
// legs differ ONLY in how Session::advance() slices the run window. All
// comparisons are exact; no approximate equality.
//
// The leg deliberately runs without infra routing (Factory has no Router)
// and with an empty base-transaction stream: both are constant across legs,
// so they cannot mask a window-size dependence, and they keep the gate
// focused on the Session itself.

namespace pl = ::PhantomLedger;
namespace entityStage = pl::pipeline::stages::entities;
namespace legitBlueprints = pl::transfers::legit::blueprints;
namespace legitLedger = pl::transfers::legit::ledger;
namespace routineSpending = pl::transfers::legit::routines::spending;
namespace plSim = pl::activity::spending::simulator;

struct SessionLeg {
  std::string digest;
  std::vector<pl::transactions::Transaction> rows;
  std::uint64_t cardEvents = 0;
};

[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

// One complete leg: fresh world, fresh session, one window partition.
// monthsPerWindow == 0 selects the full-range leg (a single advance()).
[[nodiscard]] SessionLeg runSessionLeg(const pl::synth::pii::PoolSet &poolSet,
                                       std::uint64_t seed,
                                       pl::time::Window window,
                                       int monthsPerWindow) {
  constexpr std::int32_t kPopulation = 300;

  auto rng = pl::random::Rng::fromSeed(seed);

  const pl::synth::pii::IdentityContext identity{
      .pools = &poolSet,
      .simStart = window.start,
      .localeMix = pl::synth::pii::LocaleMix::usOnly(),
  };

  // Mirrors SimulationPipeline::buildEntities() with default plans.
  pl::pipeline::People people;
  pl::pipeline::Holdings holdings;
  pl::pipeline::Counterparties cps;

  people.roster = entityStage::buildPeople(rng, kPopulation);
  holdings.accounts =
      entityStage::buildAccounts(rng, people.roster, kPopulation);
  people.personas = entityStage::buildPersonas(rng, people.roster);
  people.pii =
      entityStage::buildPii(rng, people.personas, identity,
                            people.roster.topology, pl::synth::pii::Sharing{});
  cps.merchants = entityStage::buildMerchants(rng, kPopulation);
  cps.landlords = entityStage::buildLandlords(rng, kPopulation);
  cps.counterparties = entityStage::buildCounterparties(rng, kPopulation);
  holdings.creditCards =
      entityStage::issueCreditCards(people.personas, people.roster, seed);
  entityStage::finalizeAccountRegistry(holdings, cps, people);
  entityStage::synthesizeBusinessOwners(holdings, people, rng);

  // Mirrors LegitTransferBuilder::build()'s blueprint construction.
  const legitBlueprints::LegitTimeframe timeframe{
      .window = window,
      .seed = seed,
  };
  const legitBlueprints::AccountCensus census{
      .accounts = &holdings.accounts.registry,
      .ownership = &holdings.accounts.ownership,
  };

  auto plan = legitBlueprints::buildLegitBlueprint(timeframe, census);
  plan.addCounterparties(rng, census,
                         legitBlueprints::CounterpartyPools{
                             .directory = &cps.counterparties,
                             .landlords = &cps.landlords.roster,
                         },
                         legitBlueprints::HubSelectionRules{
                             .populationCount = people.roster.roster.count,
                             .fraction = 0.01,
                         })
      .addPersonas(rng, timeframe,
                   legitBlueprints::PersonaCatalog{.pack = &people.personas});

  const legitLedger::OpeningBook openingBook{
      rng,
      legitLedger::OpeningBook::Accounts{
          .registry = &holdings.accounts.registry,
          .lookup = &holdings.accounts.lookup,
          .ownership = &holdings.accounts.ownership,
      },
      legitLedger::OpeningBook::Protections{
          .balanceRules = &pl::clearing::kDefaultBalanceRules,
          .portfolios = &holdings.portfolios,
          .creditCards = &holdings.creditCards,
      },
  };
  const auto initialBook = openingBook.build(plan);
  legitLedger::ScreenBook screen{initialBook.get()};
  auto *screenBook = screen.fresh();
  PL_CHECK(screenBook != nullptr);

  const routineSpending::SpendingRoutine routine;
  const routineSpending::SpendingRoutine::CensusSource censusSource{
      .blueprint = plan,
      .accounts =
          routineSpending::SpendingRoutine::AccountSource{
              .lookup = holdings.accounts.lookup,
              .registry = holdings.accounts.registry,
          },
  };

  auto market =
      routine.prepareMarket(censusSource,
                            routineSpending::SpendingRoutine::PayeeDirectory{
                                .merchants = &cps.merchants,
                                .creditCards = &holdings.creditCards,
                            },
                            std::span<const pl::transactions::Transaction>{});

  const auto obligations = routineSpending::SpendingRoutine::prepareObligations(
      censusSource,
      routineSpending::SpendingRoutine::ObligationSource{
          .portfolios = &holdings.portfolios,
      },
      std::span<const pl::transactions::Transaction>{},
      /*baseTxnsSorted=*/true);

  // Mirrors passes.cpp's buildCardLifecycleConfig().
  routineSpending::SessionInputs inputs;
  inputs.threadCount = 1;

  auto &cardCfg = inputs.cardLifecycle;
  cardCfg.cards = &holdings.creditCards;
  cardCfg.rules = &pl::transfers::credit_cards::kDefaultLifecycleRules;
  cardCfg.issuerAccount = plan.counterparties().issuerAcct;
  cardCfg.window = window;
  cardCfg.seed = seed;
  cardCfg.primaryAccounts.reserve(plan.primaryAcctRecordIx().size());
  for (const auto &kv : plan.primaryAcctRecordIx()) {
    const auto &record = plan.allAccounts()->records[kv.second];
    cardCfg.primaryAccounts.emplace(kv.first, record.id);
  }

  const pl::transactions::Factory txf(rng);

  const auto bundle = routineSpending::SessionBundle::make(
      seed, rng, txf, market, obligations, screenBook, std::move(inputs));

  auto &session = bundle->session();

  SessionLeg leg;
  pl::time::TimePoint expectedStart = window.start;
  int finalizedDays = 0;

  const auto consume = [&](plSim::WindowOutput output) {
    if (output.finalizedWindow.days > 0) {
      PL_CHECK(output.finalizedWindow.start == expectedStart);
      expectedStart = output.finalizedWindow.endExcl();
      finalizedDays += output.finalizedWindow.days;
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
      std::fprintf(
          stderr,
          "  first differing row %zu:\n"
          "    full-range: ts=%lld src=%llu dst=%llu amt=%.10g ch=%u\n"
          "    windowed:   ts=%lld src=%llu dst=%llu amt=%.10g ch=%u\n",
          i, static_cast<long long>(lhs.timestamp),
          static_cast<unsigned long long>(lhs.source.number),
          static_cast<unsigned long long>(lhs.target.number), lhs.amount,
          static_cast<unsigned>(lhs.session.channel.value),
          static_cast<long long>(rhs.timestamp),
          static_cast<unsigned long long>(rhs.source.number),
          static_cast<unsigned long long>(rhs.target.number), rhs.amount,
          static_cast<unsigned>(rhs.session.channel.value));
      return;
    }
  }
  std::fprintf(stderr,
               "  no differing row in the common prefix; row counts %zu vs "
               "%zu\n",
               a.size(), b.size());
}

void checkLegMatchesReference(const char *label, const SessionLeg &reference,
                              const SessionLeg &leg) {
  const bool equal = leg.digest == reference.digest &&
                     leg.rows.size() == reference.rows.size() &&
                     leg.cardEvents == reference.cardEvents;
  if (!equal) {
    std::fprintf(stderr,
                 "[session-invariance] %s diverges from full range:\n"
                 "  rows: %zu vs %zu\n"
                 "  cardEvents: %llu vs %llu\n"
                 "  digest: %s vs %s\n",
                 label, leg.rows.size(), reference.rows.size(),
                 static_cast<unsigned long long>(leg.cardEvents),
                 static_cast<unsigned long long>(reference.cardEvents),
                 leg.digest.c_str(), reference.digest.c_str());
    reportFirstRowDifference(reference.rows, leg.rows);
    PL_CHECK(equal);
  }
  std::printf("  PASS: session invariance — %s matches full range\n", label);
}

void testSessionWindowInvariance() {
  constexpr std::uint64_t seed = 20260716;

  pl::time::Window window;
  window.start = pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2; // several 12-month windows; many 1-month windows

  const auto poolSet = buildPoolSet(seed);

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
  std::printf("=== Spending Pipeline Tests ===\n");
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

  std::printf("=== Spending Session Window Invariance ===\n");
  testSessionWindowInvariance();

  std::printf("All spending pipeline tests passed.\n\n");
  return 0;
}
