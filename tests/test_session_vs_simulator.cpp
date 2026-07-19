//
// tests/test_session_vs_simulator.cpp
//
// Session vs Simulator raw-output equivalence, staged (step 12
// decomposition).
//
// History: leg 3 of this gate identified the root cause of the
// architecture-equivalence gap — the Router's mutable sticky device/IP
// state made routing order-dependent, and the windowed composition routed
// the product schedule BEFORE spending while the monolithic path routes it
// AFTER. Products now route from a pristine router snapshot on both paths
// (TransferStage::infra() / GateWorld), which this gate verifies:
//
//   leg 1  simulator            SpendingRoutine::run()
//   leg 2  session/plain        advance(full) + finish(), nothing else
//   leg 3  session/pre-moves    identical to leg 2, but first replicate
//                               EVERYTHING runLeg() does between building
//                               the session and advancing it (moved
//                               streams, product source from the pristine
//                               router snapshot, books, driver, injector).
//                               The driver is NOT run.
//   leg 4  session/in-driver    the full runLeg() Phase A (fraud off,
//                               equivalence configuration); its
//                               PhaseAResult.legitRows is the number of
//                               rows the session returned to the driver.
//
// Reading the verdicts:
//   leg2 != leg1          session behavioral gap
//   leg3 != leg2          a pre-advance construction perturbs the session
//   leg4 != leg3 (count)  the divergence happens DURING runPhaseA
//
// HARD-ENFORCED: all stages went green after the pristine-router product
// snapshot. Any divergence now FAILS with channel histograms and first
// differing rows.
//

#include "window_leg_support.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using pltest::Txn;

// Runs one leg on the shared GateWorld prefix (gate_world.hpp) — the
// exact world runLeg() builds in the equivalence configuration. Modes:
//   useSession=false                  -> SpendingRoutine::run()
//   useSession=true, preMoves=false   -> plain advance+finish
//   useSession=true, preMoves=true    -> replicate runLeg()'s pre-advance
//                                        constructions, then advance+finish
[[nodiscard]] std::vector<Txn>
runSpendingLeg(const pltest::pl::synth::pii::PoolSet &poolSet,
               std::uint64_t seed, pltest::pl::time::Window window,
               bool useSession, bool preMoves) {
  namespace pl = pltest::pl;
  namespace xfer = pltest::xfer;
  namespace routineSpending = pltest::routineSpending;
  namespace fraud_ns = pltest::fraud_ns;

  pltest::WorldSpec spec;
  spec.seed = seed;
  spec.window = window;
  spec.population = 300;
  spec.fraudProfile = pltest::scaledFraudProfile();
  spec.withBaseRoutines = true;
  // Products, infra, routing and income stay on (the spec defaults):
  // this is the equivalence configuration.

  pltest::GateWorld world(poolSet, spec);

  if (!useSession) {
    routineSpending::SpendingRoutine routine;
    routine.cardLifecycle(world.cardCfg);
    return routine.run(
        routineSpending::SpendingRoutine::Execution{
            .rng = world.rng,
            .txf = *world.txf,
            .seed = seed,
        },
        world.market, world.obligations, world.screenBook);
  }

  routineSpending::SessionInputs inputs;
  inputs.cardLifecycle = world.cardCfg;
  // threadCount left unset: machine-resolved, matching SpendingRoutine::run.

  const auto bundle = routineSpending::SessionBundle::make(
      seed, world.rng, *world.txf, world.market, world.obligations,
      world.screenBook, std::move(inputs));

  auto &session = bundle->session();

  // runLeg()'s pre-advance constructions, replicated verbatim so any one
  // of them perturbing the session shows up as leg3 != leg2. The driver is
  // built and wired but never run.
  std::unique_ptr<xfer::PrecomputedCursorSource> baseSource;
  std::unique_ptr<xfer::PrecomputedCursorSource> productSource;
  std::unique_ptr<xfer::WindowedTransferDriver> driver;

  if (preMoves) {
    const pl::random::RngFactory rngFactory{seed};

    baseSource = std::make_unique<xfer::PrecomputedCursorSource>(
        world.streams.takeReplayReady());

    const pl::transactions::Factory productTxf(world.rng, &world.productRouter,
                                               &world.infra.ringInfra);
    productSource = xfer::makeProductSource(
        window, seed, rngFactory, productTxf, world.holdings,
        pl::transfers::insurance::ClaimRates{});

    auto preBook =
        std::make_unique<pl::clearing::Ledger>(world.initialBook->clone());
    auto postBook =
        std::make_unique<pl::clearing::Ledger>(world.initialBook->clone());

    xfer::WindowedConfig config;
    config.generation.monthsPerChunk = 1'200;
    config.generation.lookaheadDays = 35;
    config.settlement.monthsPerChunk = 1;
    config.settlement.lookaheadDays = 6;

    driver = std::make_unique<xfer::WindowedTransferDriver>(
        std::move(preBook), std::move(postBook), rngFactory, config);
    driver->session(session);
    driver->addCursorSource(*baseSource);
    driver->addCursorSource(*productSource);

    // Fraud boundary objects (constructed in runLeg before the run).
    xfer::FraudEmission fraudEmission;
    fraudEmission.profile(&world.fraudProfile);
    fraudEmission.behavior(&fraud_ns::kDefaultBehavior);

    const fraud_ns::Injector injector{
        fraud_ns::InjectorServices{
            .rng = world.rng,
            .router = &world.infra.router,
            .ringInfra = &world.infra.ringInfra,
            .fraudSeed = seed ^ 0x9E3779B97F4A7C15ULL,
        },
        fraudEmission.ringView(world.people.roster.topology),
        xfer::FraudEmission::accountView(world.holdings.accounts.registry,
                                         world.holdings.accounts.ownership),
        fraudEmission.resolvedBehavior(),
    };
    (void)injector;
  }

  std::vector<Txn> rows;
  auto first = session.advance(window);
  rows.insert(rows.end(), std::make_move_iterator(first.txns.begin()),
              std::make_move_iterator(first.txns.end()));
  auto tail = session.finish();
  rows.insert(rows.end(), std::make_move_iterator(tail.txns.begin()),
              std::make_move_iterator(tail.txns.end()));
  return rows;
}

[[nodiscard]] std::string digestOf(const std::vector<Txn> &rows,
                                   pltest::pl::time::Window window) {
  const auto wrap =
      pltest::pl::pipeline::chunk::Schedule::unpartitioned(window);
  pltest::pl::exporter::sinks::Golden golden;
  golden.beginSpan(*wrap.begin());
  golden.append(std::span<const Txn>(rows.data(), rows.size()));
  golden.endSpan(*wrap.begin());
  golden.finish();
  return golden.digest();
}

void printChannelHistogramDelta(const char *labelA, const char *labelB,
                                const std::vector<Txn> &a,
                                const std::vector<Txn> &b) {
  std::map<unsigned, std::pair<std::size_t, std::size_t>> byChannel;
  for (const auto &txn : a) {
    ++byChannel[static_cast<unsigned>(txn.session.channel.value)].first;
  }
  for (const auto &txn : b) {
    ++byChannel[static_cast<unsigned>(txn.session.channel.value)].second;
  }

  std::fprintf(stderr,
               "  per-channel counts (only channels that differ):\n"
               "    channel   %-12s %-12s delta\n",
               labelA, labelB);
  bool any = false;
  for (const auto &[channel, counts] : byChannel) {
    if (counts.first == counts.second) {
      continue;
    }
    any = true;
    std::fprintf(stderr, "    0x%02x      %-12zu %-12zu %+lld\n", channel,
                 counts.first, counts.second,
                 static_cast<long long>(counts.second) -
                     static_cast<long long>(counts.first));
  }
  if (!any) {
    std::fprintf(stderr, "    (none — every channel count matches)\n");
  }
}

[[nodiscard]] bool compareLegs(const char *what, const char *labelA,
                               const char *labelB, const std::vector<Txn> &a,
                               const std::vector<Txn> &b) {
  const auto auditLess = [](const Txn &x, const Txn &y) {
    return pltest::pl::transactions::detail::auditKey(x) <
           pltest::pl::transactions::detail::auditKey(y);
  };

  auto ca = a;
  auto cb = b;
  std::sort(ca.begin(), ca.end(), auditLess);
  std::sort(cb.begin(), cb.end(), auditLess);

  bool equal = ca.size() == cb.size();
  if (equal) {
    for (std::size_t i = 0; i < ca.size(); ++i) {
      if (pltest::pl::transactions::detail::auditKey(ca[i]) !=
          pltest::pl::transactions::detail::auditKey(cb[i])) {
        equal = false;
        break;
      }
    }
  }

  if (equal) {
    std::printf("  PASS: %s — identical row multiset (%zu rows)\n", what,
                a.size());
    std::fflush(stdout);
    return true;
  }

  std::fprintf(stderr,
               "[session-vs-simulator] %s: DIVERGENCE\n"
               "  rows: %zu (%s) vs %zu (%s)\n",
               what, a.size(), labelA, b.size(), labelB);
  printChannelHistogramDelta(labelA, labelB, a, b);
  std::fprintf(stderr, "  first differing row (canonical audit order):\n");
  pltest::reportFirstRowDifference(ca, cb);
  return false;
}

} // namespace

int main() {
  std::printf("=== Session vs Simulator, Staged (driver-context bisect) ===\n");

  constexpr std::uint64_t seed = 20260720; // same world as the equivalence gate

  pltest::pl::time::Window window;
  window.start = pltest::pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::announceLeg("leg 1: simulator");
  const auto simulatorRows =
      runSpendingLeg(poolSet, seed, window, /*useSession=*/false, false);
  std::printf("  leg 1 simulator:        rows=%zu\n", simulatorRows.size());
  std::fflush(stdout);

  pltest::announceLeg("leg 2: session, plain");
  const auto sessionPlain =
      runSpendingLeg(poolSet, seed, window, /*useSession=*/true, false);
  std::printf("  leg 2 session/plain:    rows=%zu digest=%s\n",
              sessionPlain.size(), digestOf(sessionPlain, window).c_str());
  std::fflush(stdout);

  pltest::announceLeg("leg 3: session, after runLeg pre-advance moves");
  const auto sessionPreMoves =
      runSpendingLeg(poolSet, seed, window, /*useSession=*/true, true);
  std::printf("  leg 3 session/pre-moves: rows=%zu digest=%s\n",
              sessionPreMoves.size(), digestOf(sessionPreMoves, window).c_str());
  std::fflush(stdout);

  pltest::announceLeg("leg 4: session inside runPhaseA (fraud off)");
  pltest::LegOptions options;
  options.seed = seed;
  options.window = window;
  options.generationMonths = 0;
  options.settlementLookaheadDays = 6;
  options.withBaseRoutines = true;
  options.threadCount = 0; // machine-resolved
  options.withFraud = false;
  const auto driverLeg = pltest::runLeg(poolSet, options);
  std::printf("  leg 4 session/in-driver: legitRows=%llu (rows the session "
              "returned to the driver)\n",
              static_cast<unsigned long long>(driverLeg.legitRows));
  std::fflush(stdout);

  PL_CHECK(!simulatorRows.empty());
  PL_CHECK(!sessionPlain.empty());
  PL_CHECK(!sessionPreMoves.empty());

  bool allEqual = true;

  allEqual &= compareLegs("leg2 vs leg1 (session vs simulator)", "simulator",
                          "session", simulatorRows, sessionPlain);

  allEqual &= compareLegs("leg3 vs leg2 (pre-advance constructions)", "plain",
                          "pre-moves", sessionPlain, sessionPreMoves);

  if (driverLeg.legitRows != sessionPreMoves.size()) {
    std::fprintf(
        stderr,
        "[session-vs-simulator] leg4 vs leg3: the session returned %llu rows "
        "inside runPhaseA but %zu rows when advanced directly after the same "
        "constructions. The divergence happens DURING the driver's "
        "advance/settle loop, not in construction.\n",
        static_cast<unsigned long long>(driverLeg.legitRows),
        sessionPreMoves.size());
    allEqual = false;
  } else {
    std::printf("  PASS: leg4 vs leg3 — session returned the same row count "
                "inside the driver (%llu)\n",
                static_cast<unsigned long long>(driverLeg.legitRows));
    std::fflush(stdout);
  }

  if (allEqual) {
    std::printf("ALL STAGES EQUAL: session output is unperturbed by driver "
                "construction and execution.\n");
    return 0;
  }

  std::fprintf(stderr, "[session-vs-simulator] HARD FAILURE: session/"
                       "simulator equivalence regressed\n");
  return EXIT_FAILURE;
}
