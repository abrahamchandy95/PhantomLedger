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
// (TransferStage::infra() / the harness), which this gate verifies:
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

// Mirrors the runLeg() world-construction prefix in window_leg_support.hpp;
// keep the two in sync. Modes:
//   useSession=false                  -> SpendingRoutine::run()
//   useSession=true, preMoves=false   -> plain advance+finish
//   useSession=true, preMoves=true    -> replicate runLeg()'s pre-advance
//                                        constructions, then advance+finish
[[nodiscard]] std::vector<Txn>
runSpendingLeg(const pltest::pl::synth::pii::PoolSet &poolSet,
               std::uint64_t seed, pltest::pl::time::Window window,
               bool useSession, bool preMoves) {
  namespace pl = pltest::pl;
  namespace entityStage = pltest::entityStage;
  namespace infraStage = pltest::infraStage;
  namespace productStage = pltest::productStage;
  namespace xfer = pltest::xfer;
  namespace legitBlueprints = pltest::legitBlueprints;
  namespace legitLedger = pltest::legitLedger;
  namespace legitPasses = pltest::legitPasses;
  namespace routineSpending = pltest::routineSpending;
  namespace fraud_ns = pltest::fraud_ns;
  namespace cpKeys = pltest::cpKeys;

  constexpr std::int32_t kPopulation = 300;

  auto rng = pl::random::Rng::fromSeed(seed);

  const pl::synth::pii::IdentityContext identity{
      .pools = &poolSet,
      .simStart = window.start,
      .localeMix = pl::synth::pii::LocaleMix::usOnly(),
  };

  const auto fraudProfile = pltest::scaledFraudProfile();

  pl::pipeline::People people;
  pl::pipeline::Holdings holdings;
  pl::pipeline::Counterparties cps;

  people.roster = entityStage::buildPeople(rng, kPopulation, fraudProfile);
  holdings.accounts =
      entityStage::buildAccounts(rng, people.roster, kPopulation);
  people.personas = entityStage::buildPersonas(rng, people.roster);
  people.pii = entityStage::buildPii(rng, people.personas, identity,
                                     people.roster.topology,
                                     pl::synth::pii::Sharing{});
  cps.merchants = entityStage::buildMerchants(rng, kPopulation);
  cps.landlords = entityStage::buildLandlords(rng, kPopulation);
  cps.counterparties = entityStage::buildCounterparties(rng, kPopulation);
  holdings.creditCards =
      entityStage::issueCreditCards(people.personas, people.roster, seed);
  entityStage::finalizeAccountRegistry(holdings, cps, people);
  entityStage::synthesizeBusinessOwners(holdings, people, rng);

  productStage::ObligationSynthesis{}.synthesize(people, holdings, window);

  const auto infra =
      infraStage::AccessInfraStage{}.build(rng, people, holdings, window);

  // Pristine router snapshot for product routing (see the harness comment).
  const pl::infra::Router productRouter = infra.router;

  const legitBlueprints::LegitTimeframe timeframe{
      .window = window,
      .seed = seed,
  };
  const legitBlueprints::AccountCensus census{
      .accounts = &holdings.accounts.registry,
      .ownership = &holdings.accounts.ownership,
  };

  auto plan = legitBlueprints::buildLegitBlueprint(timeframe, census);
  plan.addCounterparties(
          rng, census,
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
  PL_CHECK(initialBook != nullptr);

  legitLedger::ScreenBook screen{initialBook.get()};

  const pl::transactions::Factory txf(rng, &infra.router);

  const legitPasses::AccountAccess accountAccess{
      .registry = &holdings.accounts.registry,
      .ownership = &holdings.accounts.ownership,
  };

  const pl::transfers::government::RetirementTerms retirement{};
  const pl::transfers::government::DisabilityTerms disability{};

  legitLedger::TxnStreams streams;
  {
    const legitPasses::IncomePass incomePass{
        &rng,
        accountAccess,
        legitPasses::SalarySetup{
            .revenueCounterparties = &cps.counterparties,
            .rules = {},
        },
        legitPasses::GovernmentSetup{
            .counterparties =
                legitPasses::GovernmentCounterparties{
                    .ssa = cpKeys::key(cpKeys::Government::ssa),
                    .disability = cpKeys::key(cpKeys::Government::disability),
                },
            .retirement = &retirement,
            .disability = &disability,
        },
    };
    legitPasses::addIncome(incomePass, plan, txf, streams);
  }

  {
    auto routinePass = legitPasses::RoutinePass{
        &rng,
        accountAccess,
        legitPasses::RoutineResources{
            .accountsLookup = &holdings.accounts.lookup,
            .merchants = &cps.merchants,
            .portfolios = &holdings.portfolios,
            .creditCards = &holdings.creditCards,
            .cardLifecycle = &pl::transfers::credit_cards::kDefaultLifecycleRules,
        },
    };
    routinePass.txf(txf);
    legitPasses::addRoutinesWithoutSpending(routinePass, plan, streams, screen);
  }

  const routineSpending::SpendingRoutine::CensusSource censusSource{
      .blueprint = plan,
      .accounts =
          routineSpending::SpendingRoutine::AccountSource{
              .lookup = holdings.accounts.lookup,
              .registry = holdings.accounts.registry,
          },
  };

  const std::span<const Txn> baseTxns(streams.screened());

  const routineSpending::SpendingRoutine routineForPrep;
  auto market = routineForPrep.prepareMarket(
      censusSource,
      routineSpending::SpendingRoutine::PayeeDirectory{
          .merchants = &cps.merchants,
          .creditCards = &holdings.creditCards,
      },
      baseTxns);

  const auto obligations = routineSpending::SpendingRoutine::prepareObligations(
      censusSource,
      routineSpending::SpendingRoutine::ObligationSource{
          .portfolios = &holdings.portfolios,
      },
      baseTxns,
      /*baseTxnsSorted=*/true);

  auto *screenBook = screen.fresh();
  PL_CHECK(screenBook != nullptr);

  // Mirrors passes.cpp's buildCardLifecycleConfig().
  routineSpending::SpendingRoutine::CardLifecycleConfig cardCfg;
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

  if (!useSession) {
    routineSpending::SpendingRoutine routine;
    routine.cardLifecycle(cardCfg);
    return routine.run(
        routineSpending::SpendingRoutine::Execution{
            .rng = rng,
            .txf = txf,
            .seed = seed,
        },
        market, obligations, screenBook);
  }

  routineSpending::SessionInputs inputs;
  inputs.cardLifecycle = cardCfg;
  // threadCount left unset: machine-resolved, matching SpendingRoutine::run.

  const auto bundle = routineSpending::SessionBundle::make(
      seed, rng, txf, market, obligations, screenBook, std::move(inputs));

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
        streams.takeReplayReady());

    const pl::transactions::Factory productTxf(rng, &productRouter,
                                               &infra.ringInfra);
    productSource = xfer::makeProductSource(
        window, seed, rngFactory, productTxf, holdings,
        pl::transfers::insurance::ClaimRates{});

    auto preBook =
        std::make_unique<pl::clearing::Ledger>(initialBook->clone());
    auto postBook =
        std::make_unique<pl::clearing::Ledger>(initialBook->clone());

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
    fraudEmission.profile(&fraudProfile);
    fraudEmission.behavior(&fraud_ns::kDefaultBehavior);

    const fraud_ns::Injector injector{
        fraud_ns::InjectorServices{
            .rng = rng,
            .router = &infra.router,
            .ringInfra = &infra.ringInfra,
            .fraudSeed = seed ^ 0x9E3779B97F4A7C15ULL,
        },
        fraudEmission.ringView(people.roster.topology),
        xfer::FraudEmission::accountView(holdings.accounts.registry,
                                         holdings.accounts.ownership),
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
