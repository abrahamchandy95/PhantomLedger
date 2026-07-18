#pragma once
//
// tests/window_leg_support.hpp
//
// Shared world/leg harness for the windowed two-phase driver gates
// (test_window_invariance, test_window_bisect, test_thread_invariance,
// test_order_ties, test_arch_equivalence, test_session_vs_simulator,
// test_spool_equivalence).
//
// runLeg() builds ONE complete, fresh, deterministically seeded world —
// entities, infra, blueprint, opening books, income, base routines,
// market/obligations, persistent spending session with card lifecycle,
// family, product and fraud sources — and folds it through
// WindowedTransferDriver. Legs are compared with the acceptance
// RunFingerprint. Every option in LegOptions is leg-constant by
// construction; only generationMonths (window gates) or threadCount
// (thread gate) may differ between legs of one gate.
//
// LegOptions toggles exist so the bisect test can isolate layers and the
// equivalence test can match the monolithic composition:
//   withInfraRouting   device/IP routing draws on the shared RNG per row
//   withIncome         salary/government/revenue rows via a cursor source
//   withBaseRoutines   split deposits, rent, subscriptions, ATM, internal
//                      transfers via passes::addRoutinesWithoutSpending —
//                      the exact monolithic base-stream generation
//   withFamily         family transfers via the dedicated family lanes
//                      and a pristine router copy (mirrors builder.cpp)
//   withProducts       portfolio synthesis + precomputed product schedule
//   withFraud          two-phase run; false folds Phase A only
//   useBinarySpool     candidates cross the Phase A / Phase B boundary
//                      through a file-backed binary spool instead of the
//                      in-memory vector (byte-identical by the spool gate)
//
// ROUTER SNAPSHOTS: the Router carries mutable sticky per-person device/IP
// state, so routing is order-dependent across generators. Products and
// family each route from their own pristine copy taken immediately after
// infra build (mirroring TransferStage::infra() and
// LegitTransferBuilder::build()), so neither can perturb — or be
// perturbed by — the sticky state the session shares with income and
// routines.
//
// FRAUD PROFILE: the production default plans ~6 rings per 10,000 people
// and the count ROUNDS, so a 300-person world plans zero fraud
// participants and every fraud-boundary gate would be vacuous. runLeg()
// therefore uses a test-scaled profile: the ring RATE is raised and its
// sigma zeroed (deterministic count, no extra RNG draw), which guarantees
// at least one ring at this population. The fraud BUDGET — targetTxnFraudP
// and the realized-corpus ratio — is untouched. This is leg-constant test
// configuration, not a model change. Raise LegOptions.population for soak
// runs; the profile scales safely with it.
//

#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/pipeline/acceptance/fingerprint.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/products.hpp"
#include "phantomledger/pipeline/stages/transfers/binary_spool.hpp"
#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/window_sources.hpp"
#include "phantomledger/pipeline/stages/transfers/windowed_driver.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/relationships/family/support.hpp"
#include "phantomledger/synth/people/fraud.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/counterparties/accounts.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/channels/government/disability.hpp"
#include "phantomledger/transfers/channels/government/retirement.hpp"
#include "phantomledger/transfers/channels/insurance/rates.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/legit/routines/spending.hpp"
#include "phantomledger/transfers/legit/routines/spending_session.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pltest {

namespace pl = ::PhantomLedger;
namespace entityStage = pl::pipeline::stages::entities;
namespace infraStage = pl::pipeline::stages::infra;
namespace productStage = pl::pipeline::stages::products;
namespace xfer = pl::pipeline::stages::transfers;
namespace acceptance = pl::pipeline::acceptance;
namespace legitBlueprints = pl::transfers::legit::blueprints;
namespace legitLedger = pl::transfers::legit::ledger;
namespace legitPasses = pl::transfers::legit::ledger::passes;
namespace routineSpending = pl::transfers::legit::routines::spending;
namespace fraud_ns = pl::transfers::fraud;
namespace cpKeys = pl::counterparties;

using Txn = pl::transactions::Transaction;

// Golden digest plus retained rows so a digest mismatch can be localized
// to the first differing transaction.
struct CapturingGolden {
  void beginSpan(const pl::pipeline::chunk::Span &span) {
    golden.beginSpan(span);
  }

  void append(std::span<const Txn> txns) {
    golden.append(txns);
    rows.insert(rows.end(), txns.begin(), txns.end());
  }

  void endSpan(const pl::pipeline::chunk::Span &span) { golden.endSpan(span); }

  void finish() { golden.finish(); }

  [[nodiscard]] std::uint64_t rowsWritten() const {
    return golden.rowsWritten();
  }

  pl::exporter::sinks::Golden golden;
  std::vector<Txn> rows;
};

struct LegOptions {
  std::uint64_t seed = 0;
  pl::time::Window window{};

  // 0 selects one full-range generation window.
  int generationMonths = 0;

  // Monthly settlement spans; the lookahead must match the settlement
  // strategy of whatever this leg is compared against. The windowed gates
  // use 35 days; the equivalence gate must use the monolithic default (6).
  int settlementLookaheadDays = 35;

  std::int32_t population = 300;

  // Emission worker count. 0 leaves the count machine-resolved — exactly
  // what SpendingRoutine::run does — so architecture-equivalence legs run
  // spending at the same thread count as the monolithic path. Invariance
  // gates pin explicit counts.
  std::uint32_t threadCount = 1;

  bool withInfraRouting = true;
  bool withIncome = true;
  bool withBaseRoutines = false;
  bool withFamily = false;
  bool withProducts = true;
  bool withFraud = true;

  // Candidate spool for the two-phase run: false composes via
  // driver.runTwoPhase (in-memory vector spool); true composes the same
  // phases manually around a file-backed BinaryCandidateSpool. Output
  // must be byte-identical (test_spool_equivalence).
  bool useBinarySpool = false;
};

struct LegResult {
  acceptance::RunFingerprint fingerprint;
  std::vector<Txn> rows;

  std::uint64_t legitRows = 0;
  std::uint64_t sourceRows = 0;

  // Cursor-source accounting. `remaining` counts generated rows the driver
  // NEVER staged (timestamps at/beyond the final finalized coverage); a
  // nonzero value means the leg silently lost source rows the monolithic
  // fold would have settled in its unbounded final span.
  std::uint64_t baseSourceEmitted = 0;
  std::uint64_t baseSourceRemaining = 0;
  std::uint64_t productSourceEmitted = 0;
  std::uint64_t productSourceRemaining = 0;

  // Binary-spool accounting (spool legs only): candidate rows and bytes
  // that crossed the Phase A / Phase B boundary through the file.
  std::uint64_t spoolRows = 0;
  std::uint64_t spoolBytes = 0;
};

[[nodiscard]] inline pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

// See FRAUD PROFILE in the file comment. With participation ceiling
// 0.10 * population >= size.min, the first sampleRing() call cannot fail,
// so at least one ring (with mules and victims) exists at any seed.
[[nodiscard]] inline pl::synth::people::Fraud scaledFraudProfile() {
  pl::synth::people::Fraud profile{};
  profile.rings.perTenKMean = 200.0;
  profile.rings.perTenKSigma = 0.0;
  profile.solos.perTenK = 100.0;
  profile.limits.maxParticipationP = 0.10;
  return profile;
}

[[nodiscard]] inline LegResult runLeg(const pl::synth::pii::PoolSet &poolSet,
                                      const LegOptions &opt) {
  // The injector reads ring device/IP plans from infra.
  PL_CHECK(!opt.withFraud || opt.withInfraRouting);
  // Split deposits consume the payday-inbound stream that income emits.
  PL_CHECK(!opt.withBaseRoutines || opt.withIncome);

  auto rng = pl::random::Rng::fromSeed(opt.seed);

  const pl::synth::pii::IdentityContext identity{
      .pools = &poolSet,
      .simStart = opt.window.start,
      .localeMix = pl::synth::pii::LocaleMix::usOnly(),
  };

  const auto fraudProfile = scaledFraudProfile();

  // Mirrors SimulationPipeline::buildEntities() with default plans.
  pl::pipeline::People people;
  pl::pipeline::Holdings holdings;
  pl::pipeline::Counterparties cps;

  people.roster = entityStage::buildPeople(rng, opt.population, fraudProfile);
  holdings.accounts =
      entityStage::buildAccounts(rng, people.roster, opt.population);
  people.personas = entityStage::buildPersonas(rng, people.roster);
  people.pii = entityStage::buildPii(rng, people.personas, identity,
                                     people.roster.topology,
                                     pl::synth::pii::Sharing{});
  cps.merchants = entityStage::buildMerchants(rng, opt.population);
  cps.landlords = entityStage::buildLandlords(rng, opt.population);
  cps.counterparties = entityStage::buildCounterparties(rng, opt.population);
  holdings.creditCards =
      entityStage::issueCreditCards(people.personas, people.roster, opt.seed);
  entityStage::finalizeAccountRegistry(holdings, cps, people);
  entityStage::synthesizeBusinessOwners(holdings, people, rng);

  if (opt.withProducts) {
    // Products use their own content-keyed seed; window size cannot reach it.
    productStage::ObligationSynthesis{}.synthesize(people, holdings,
                                                   opt.window);
  }

  // Infra is always built so shared-RNG consumption is identical across all
  // option combinations of one gate; withInfraRouting only controls whether
  // the row factory routes device/IP through it.
  const auto infra =
      infraStage::AccessInfraStage{}.build(rng, people, holdings, opt.window);

  // See ROUTER SNAPSHOTS in the file comment: taken here, before any income
  // or routine emission touches the shared router's sticky state. Products
  // and family each mutate their own copy, as in production.
  const pl::infra::Router productRouter = infra.router;
  const pl::infra::Router familyRouter = infra.router;

  // Mirrors LegitTransferBuilder::build()'s blueprint construction.
  const legitBlueprints::LegitTimeframe timeframe{
      .window = opt.window,
      .seed = opt.seed,
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

  const pl::transactions::Factory txf(
      rng, opt.withInfraRouting ? &infra.router : nullptr);

  const legitPasses::AccountAccess accountAccess{
      .registry = &holdings.accounts.registry,
      .ownership = &holdings.accounts.ownership,
  };

  // Income (salary, government benefits, revenue) mirrors passes::addIncome.
  // Its payday inbounds are what future-inbound cure discovery finds.
  const pl::transfers::government::RetirementTerms retirement{};
  const pl::transfers::government::DisabilityTerms disability{};

  legitLedger::TxnStreams streams;
  if (opt.withIncome) {
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

  if (opt.withBaseRoutines) {
    // The exact monolithic base-stream generation, minus spending.
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

  // Market and obligations read the accumulated stream exactly as
  // addSpending reads streams.screened() — family rows are deliberately
  // NOT in it (the monolithic path generates family after spending). The
  // screened view must stay untouched for the session's lifetime (the
  // obligations snapshot holds a span into it).
  const routineSpending::SpendingRoutine routine;
  const routineSpending::SpendingRoutine::CensusSource censusSource{
      .blueprint = plan,
      .accounts =
          routineSpending::SpendingRoutine::AccountSource{
              .lookup = holdings.accounts.lookup,
              .registry = holdings.accounts.registry,
          },
  };

  const std::span<const Txn> baseTxns(streams.screened());

  auto market = routine.prepareMarket(
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
  routineSpending::SessionInputs inputs;
  if (opt.threadCount != 0) {
    inputs.threadCount = opt.threadCount;
  }

  auto &cardCfg = inputs.cardLifecycle;
  cardCfg.cards = &holdings.creditCards;
  cardCfg.rules = &pl::transfers::credit_cards::kDefaultLifecycleRules;
  cardCfg.issuerAccount = plan.counterparties().issuerAcct;
  cardCfg.window = opt.window;
  cardCfg.seed = opt.seed;
  cardCfg.primaryAccounts.reserve(plan.primaryAcctRecordIx().size());
  for (const auto &kv : plan.primaryAcctRecordIx()) {
    const auto &record = plan.allAccounts()->records[kv.second];
    cardCfg.primaryAccounts.emplace(kv.first, record.id);
  }

  const auto bundle = routineSpending::SessionBundle::make(
      opt.seed, rng, txf, market, obligations, screenBook, std::move(inputs));

  // Window-independent cursor sources, precomputed at this fixed sequence
  // point in every leg. Products and family draw only from their dedicated
  // lanes and route from their pristine snapshots, so their generation
  // point cannot affect the session.
  const pl::random::RngFactory rngFactory{opt.seed};

  std::unique_ptr<xfer::PrecomputedCursorSource> baseSource;
  if (opt.withIncome) {
    baseSource = std::make_unique<xfer::PrecomputedCursorSource>(
        streams.takeReplayReady());
  }

  std::unique_ptr<xfer::PrecomputedCursorSource> productSource;
  if (opt.withProducts) {
    const pl::transactions::Factory productTxf(rng, &productRouter,
                                               &infra.ringInfra);
    productSource = xfer::makeProductSource(
        opt.window, opt.seed, rngFactory, productTxf, holdings,
        pl::transfers::insurance::ClaimRates{});
  }

  std::unique_ptr<xfer::PrecomputedCursorSource> familySource;
  if (opt.withFamily) {
    // Mirrors LegitTransferBuilder::build()'s family emission with the
    // production default scenario (assembly.cpp's makeDefaultFamilyScenario).
    namespace family_rel = pl::relationships::family;
    namespace relatives = pl::transfers::legit::routines::relatives;
    namespace family_rt = pl::transfers::legit::routines::family;

    relatives::FamilyTransferScenario scenario;
    scenario.households(family_rel::kDefaultHouseholds)
        .dependents(family_rel::kDefaultDependents)
        .retireeSupport(family_rel::kDefaultRetireeSupport)
        .transfers(relatives::kDefaultFamilyTransferModel);

    const relatives::FamilyLedgerSources sources{
        .accounts = &holdings.accounts.registry,
        .ownership = &holdings.accounts.ownership,
        .educationMerchants = &cps.merchants,
    };

    const pl::random::RngFactory familyRngFactory{opt.seed};
    auto familyRoutingRng = familyRngFactory.rng({"family", "routing"});
    const pl::transactions::Factory familyTxf(familyRoutingRng, &familyRouter);

    familySource = std::make_unique<xfer::PrecomputedCursorSource>(
        legitLedger::sortForReplay(relatives::generateFamilyTxns(
            plan, sources,
            family_rt::TransferEmission{familyRngFactory, familyTxf},
            scenario)));
  }

  // Two fresh opening-book copies: pre-fraud replay and post-fraud replay
  // fold independently, exactly as in the monolithic path.
  auto preBook = std::make_unique<pl::clearing::Ledger>(initialBook->clone());
  auto postBook = std::make_unique<pl::clearing::Ledger>(initialBook->clone());
  auto *preBookPtr = preBook.get();
  auto *postBookPtr = postBook.get();

  xfer::WindowedConfig config;
  config.generation.monthsPerChunk =
      opt.generationMonths == 0 ? 1'200 : opt.generationMonths;
  config.generation.lookaheadDays = 35;
  config.settlement.monthsPerChunk = 1;
  config.settlement.lookaheadDays = opt.settlementLookaheadDays;

  xfer::WindowedTransferDriver driver(std::move(preBook), std::move(postBook),
                                      rngFactory, config);
  driver.session(bundle->session());
  if (baseSource != nullptr) {
    driver.addCursorSource(*baseSource);
  }
  if (productSource != nullptr) {
    driver.addCursorSource(*productSource);
  }
  if (familySource != nullptr) {
    driver.addCursorSource(*familySource);
  }

  LegResult out;
  CapturingGolden sink;

  if (opt.withFraud) {
    // Fraud boundary inputs. Injector construction mirrors
    // TransferStage::makeFraudInjector, including the fraud seed derivation.
    xfer::FraudEmission fraudEmission;
    fraudEmission.profile(&fraudProfile);
    fraudEmission.behavior(&fraud_ns::kDefaultBehavior);

    const fraud_ns::Injector injector{
        fraud_ns::InjectorServices{
            .rng = rng,
            .router = &infra.router,
            .ringInfra = &infra.ringInfra,
            .fraudSeed = opt.seed ^ 0x9E3779B97F4A7C15ULL,
        },
        fraudEmission.ringView(people.roster.topology),
        xfer::FraudEmission::accountView(holdings.accounts.registry,
                                         holdings.accounts.ownership),
        fraudEmission.resolvedBehavior(),
    };

    legitLedger::LegitCounterparties legitCps;
    legitCps.hubAccounts = plan.counterparties().hubAccounts;
    legitCps.billerAccounts = plan.counterparties().billerAccounts;
    legitCps.employers = plan.counterparties().employers;

    const xfer::WindowedTransferDriver::FraudSourceFactory makeFraud =
        [&](std::uint64_t realizedCandidateCount)
        -> std::unique_ptr<xfer::ScheduleCursorSource> {
      return xfer::makeFraudSource(
          injector, opt.window,
          static_cast<std::size_t>(realizedCandidateCount),
          xfer::FraudEmission::legitCounterparties(legitCps));
    };

    xfer::RunSummary summary;
    if (opt.useBinarySpool) {
      // The exact runTwoPhase composition, with the file-backed spool at
      // the Phase A / Phase B boundary instead of the in-memory vector:
      // Phase A folds into the spool, the fraud source is built from the
      // exact realized count L, then Phase B streams the candidates back
      // out of the file.
      xfer::BinaryCandidateSpool spool;
      summary.phaseA = driver.runPhaseA(opt.window, spool);

      const auto fraudSource = makeFraud(summary.phaseA.candidateRows);

      const auto candidates = spool.openCursor();
      summary.phaseB =
          driver.runPhaseB(opt.window, *candidates, fraudSource.get(), sink);

      out.spoolRows = spool.rowsWritten();
      out.spoolBytes = spool.bytesSpooled();
    } else {
      summary = driver.runTwoPhase(opt.window, makeFraud, sink);
    }

    out.fingerprint.rows = sink.rowsWritten();
    out.fingerprint.digest = sink.golden.digest();
    out.fingerprint.candidateRows = summary.phaseA.candidateRows;
    out.fingerprint.fraudRows = summary.phaseB.fraudRows;
    out.fingerprint.cardEvents = summary.phaseA.cardEvents;
    out.fingerprint.preDropsByReason =
        acceptance::RunFingerprint::normalize(summary.phaseA.preDrops.byReason);
    out.fingerprint.preDropsByChannel = acceptance::RunFingerprint::normalize(
        summary.phaseA.preDrops.byChannel);
    out.fingerprint.postDropsByReason = acceptance::RunFingerprint::normalize(
        summary.phaseB.postDrops.byReason);
    out.fingerprint.postDropsByChannel = acceptance::RunFingerprint::normalize(
        summary.phaseB.postDrops.byChannel);
    out.fingerprint.bookHash = acceptance::hashBook(*postBookPtr);

    out.legitRows = summary.phaseA.legitRows;
    out.sourceRows = summary.phaseA.sourceRows;
  } else {
    // Phase A only: the sink receives the accepted pre-fraud candidates and
    // the fingerprint pins the pre-fraud fold plus the pre-book state.
    const auto phaseA = driver.runPhaseA(opt.window, sink);

    out.fingerprint.rows = sink.rowsWritten();
    out.fingerprint.digest = sink.golden.digest();
    out.fingerprint.candidateRows = phaseA.candidateRows;
    out.fingerprint.cardEvents = phaseA.cardEvents;
    out.fingerprint.preDropsByReason =
        acceptance::RunFingerprint::normalize(phaseA.preDrops.byReason);
    out.fingerprint.preDropsByChannel =
        acceptance::RunFingerprint::normalize(phaseA.preDrops.byChannel);
    out.fingerprint.bookHash = acceptance::hashBook(*preBookPtr);

    out.legitRows = phaseA.legitRows;
    out.sourceRows = phaseA.sourceRows;
  }

  if (baseSource != nullptr) {
    out.baseSourceEmitted = baseSource->emittedTotal();
    out.baseSourceRemaining =
        static_cast<std::uint64_t>(baseSource->remaining());
  }
  if (productSource != nullptr) {
    out.productSourceEmitted = productSource->emittedTotal();
    out.productSourceRemaining =
        static_cast<std::uint64_t>(productSource->remaining());
  }

  out.rows = std::move(sink.rows);
  return out;
}

// ctest pipes stdout with full buffering; flushing after every progress
// line is what distinguishes a slow gate from a hung one.
inline void announceLeg(const char *label) {
  std::printf("  running %s ...\n", label);
  std::fflush(stdout);
}

inline void printLeg(const char *label, const LegResult &leg) {
  std::printf("  %-28s rows=%llu L=%llu fraud=%llu cards=%llu legit=%llu "
              "source=%llu\n",
              label, static_cast<unsigned long long>(leg.fingerprint.rows),
              static_cast<unsigned long long>(leg.fingerprint.candidateRows),
              static_cast<unsigned long long>(leg.fingerprint.fraudRows),
              static_cast<unsigned long long>(leg.fingerprint.cardEvents),
              static_cast<unsigned long long>(leg.legitRows),
              static_cast<unsigned long long>(leg.sourceRows));
  std::fflush(stdout);
}

inline void reportFirstRowDifference(const std::vector<Txn> &a,
                                     const std::vector<Txn> &b) {
  const auto n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (pl::transactions::detail::auditKey(a[i]) !=
        pl::transactions::detail::auditKey(b[i])) {
      const auto &lhs = a[i];
      const auto &rhs = b[i];
      std::fprintf(
          stderr,
          "  first differing row %zu:\n"
          "    reference: ts=%lld src=%llu dst=%llu amt=%.10g ch=%u f=%u\n"
          "    windowed:  ts=%lld src=%llu dst=%llu amt=%.10g ch=%u f=%u\n",
          i, static_cast<long long>(lhs.timestamp),
          static_cast<unsigned long long>(lhs.source.number),
          static_cast<unsigned long long>(lhs.target.number), lhs.amount,
          static_cast<unsigned>(lhs.session.channel.value),
          static_cast<unsigned>(lhs.fraud.flag),
          static_cast<long long>(rhs.timestamp),
          static_cast<unsigned long long>(rhs.source.number),
          static_cast<unsigned long long>(rhs.target.number), rhs.amount,
          static_cast<unsigned>(rhs.session.channel.value),
          static_cast<unsigned>(rhs.fraud.flag));
      return;
    }
  }
  std::fprintf(stderr,
               "  no differing row in the common prefix; row counts %zu vs "
               "%zu\n",
               a.size(), b.size());
}

inline void checkLegMatches(const char *label, const LegResult &reference,
                            const LegResult &leg) {
  const auto diff =
      acceptance::firstDifference(reference.fingerprint, leg.fingerprint);

  const bool auxEqual = reference.legitRows == leg.legitRows &&
                        reference.sourceRows == leg.sourceRows;

  if (!diff.empty() || !auxEqual) {
    std::fprintf(stderr, "[window-invariance] %s diverges from reference:\n",
                 label);
    if (!diff.empty()) {
      std::fprintf(stderr, "  %s\n", diff.c_str());
    }
    if (!auxEqual) {
      std::fprintf(stderr,
                   "  legitRows %llu vs %llu, sourceRows %llu vs %llu\n",
                   static_cast<unsigned long long>(reference.legitRows),
                   static_cast<unsigned long long>(leg.legitRows),
                   static_cast<unsigned long long>(reference.sourceRows),
                   static_cast<unsigned long long>(leg.sourceRows));
    }
    reportFirstRowDifference(reference.rows, leg.rows);
    PL_CHECK(diff.empty() && auxEqual);
  }

  std::printf("  PASS: %s matches reference\n", label);
  std::fflush(stdout);
}

} // namespace pltest
