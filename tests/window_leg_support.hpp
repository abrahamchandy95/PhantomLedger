#pragma once
//
// tests/window_leg_support.hpp
//
// Shared leg harness for the windowed two-phase driver gates
// (test_window_invariance, test_window_bisect, test_thread_invariance,
// test_order_ties, test_arch_equivalence, test_session_vs_simulator,
// test_spool_equivalence).
//
// runLeg() builds ONE complete, fresh, deterministically seeded world —
// the shared GateWorld prefix (see gate_world.hpp): entities, infra,
// blueprint, opening books, income, base routines, market/obligations —
// then adds the persistent spending session with card lifecycle, the
// family, product and fraud sources, and folds it all through
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
// The ROUTER SNAPSHOTS and FRAUD PROFILE rationale lives with the world
// construction in gate_world.hpp.
//

#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/pipeline/acceptance/fingerprint.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/stages/transfers/binary_spool.hpp"
#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/window_sources.hpp"
#include "phantomledger/pipeline/stages/transfers/windowed_driver.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/relationships/family/support.hpp"
#include "phantomledger/transfers/channels/insurance/rates.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"
#include "phantomledger/transfers/legit/routines/spending_session.hpp"

#include "gate_world.hpp"
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

namespace xfer = pl::pipeline::stages::transfers;
namespace acceptance = pl::pipeline::acceptance;
namespace fraud_ns = pl::transfers::fraud;

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

[[nodiscard]] inline LegResult runLeg(const pl::synth::pii::PoolSet &poolSet,
                                      const LegOptions &opt) {
  // The injector reads ring device/IP plans from infra.
  PL_CHECK(!opt.withFraud || opt.withInfraRouting);
  // Split deposits consume the payday-inbound stream that income emits.
  PL_CHECK(!opt.withBaseRoutines || opt.withIncome);

  // Infra is always built (the WorldSpec default) so shared-RNG
  // consumption is identical across all option combinations of one gate;
  // withInfraRouting only controls whether the row factory routes
  // device/IP through it.
  WorldSpec spec;
  spec.seed = opt.seed;
  spec.window = opt.window;
  spec.population = opt.population;
  spec.fraudProfile = scaledFraudProfile();
  spec.withProducts = opt.withProducts;
  spec.withInfraRouting = opt.withInfraRouting;
  spec.withIncome = opt.withIncome;
  spec.withBaseRoutines = opt.withBaseRoutines;

  GateWorld world(poolSet, spec);
  auto &rng = world.rng;

  routineSpending::SessionInputs inputs;
  inputs.cardLifecycle = world.cardCfg;
  if (opt.threadCount != 0) {
    inputs.threadCount = opt.threadCount;
  }

  const auto bundle = routineSpending::SessionBundle::make(
      opt.seed, rng, *world.txf, world.market, world.obligations,
      world.screenBook, std::move(inputs));

  // Window-independent cursor sources, precomputed at this fixed sequence
  // point in every leg. Products and family draw only from their dedicated
  // lanes and route from their pristine snapshots, so their generation
  // point cannot affect the session.
  const pl::random::RngFactory rngFactory{opt.seed};

  std::unique_ptr<xfer::PrecomputedCursorSource> baseSource;
  if (opt.withIncome) {
    baseSource = std::make_unique<xfer::PrecomputedCursorSource>(
        world.streams.takeReplayReady());
  }

  std::unique_ptr<xfer::PrecomputedCursorSource> productSource;
  if (opt.withProducts) {
    const pl::transactions::Factory productTxf(rng, &world.productRouter,
                                               &world.infra.ringInfra);
    // The default-constructed synthesis is EXACTLY what built the gate
    // world's portfolios (gate_world.hpp) — the emitter replays it for
    // the transient whole-window obligation stream (RAM R2.2.1c).
    productSource = xfer::makeProductSource(
        opt.window, opt.seed, rngFactory, productTxf, world.people,
        world.holdings, pl::pipeline::stages::products::ObligationSynthesis{},
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
        .accounts = &world.holdings.accounts.registry,
        .ownership = &world.holdings.accounts.ownership,
        .educationMerchants = &world.cps.merchants,
    };

    const pl::random::RngFactory familyRngFactory{opt.seed};
    auto familyRoutingRng = familyRngFactory.rng({"family", "routing"});
    const pl::transactions::Factory familyTxf(familyRoutingRng,
                                              &world.familyRouter);

    familySource = std::make_unique<xfer::PrecomputedCursorSource>(
        legitLedger::sortForReplay(relatives::generateFamilyTxns(
            world.plan, sources,
            family_rt::TransferEmission{familyRngFactory, familyTxf},
            scenario)));
  }

  // Two fresh opening-book copies: pre-fraud replay and post-fraud replay
  // fold independently, exactly as in the monolithic path.
  auto preBook =
      std::make_unique<pl::clearing::Ledger>(world.initialBook->clone());
  auto postBook =
      std::make_unique<pl::clearing::Ledger>(world.initialBook->clone());
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
    // TransferStage::makeFraudInjector, including the fraud seed derivation
    // and (H3 3c-ii) the timeline carrier for the ring alive intervals.
    xfer::FraudEmission fraudEmission;
    fraudEmission.profile(&world.fraudProfile);
    fraudEmission.behavior(&fraud_ns::kDefaultBehavior);

    const fraud_ns::Injector injector{
        fraud_ns::InjectorServices{
            .rng = rng,
            .router = &world.infra.router,
            .ringInfra = &world.infra.ringInfra,
            .fraudSeed = opt.seed ^ 0x9E3779B97F4A7C15ULL,
        },
        fraudEmission.ringView(world.people.roster.topology,
                               world.people.personas.timelines),
        xfer::FraudEmission::accountView(world.holdings.accounts.registry,
                                         world.holdings.accounts.ownership),
        fraudEmission.resolvedBehavior(),
    };

    legitLedger::LegitCounterparties legitCps;
    legitCps.hubAccounts = world.plan.counterparties().hubAccounts;
    legitCps.billerAccounts = world.plan.counterparties().billerAccounts;
    legitCps.employers = world.plan.counterparties().employers;

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
