//
// src/pipeline/stages/transfers/windowed_run.cpp
//
// TransferStage::runWindowedErased — the windowed two-phase production
// composition. This is the production port of the gate harness's
// runLeg() (tests/window_leg_support.hpp), which is its executable
// specification; test_production_windowed holds the two byte-identical
// against the monolithic run().
//
// Sequence (order is output-defining for the shared sequential stream and
// therefore fixed):
//
//   1. generation prologue   blueprint, opening book, income, base
//                            routines (shared stream, live router)
//   2. spending session      market + obligations over the screened base
//                            stream, persistent Session with card driver
//   3. cursor sources        base (income+routines, disk-spooled),
//                            products (products/full_schedule lane,
//                            pristine router copy), family (family
//                            lanes, pristine router copy)
//   4. two-phase fold        Phase A -> candidate spool -> exact count L
//                            -> fraud source -> Phase B -> sink
//
// Rows stream to the caller's sink; nothing transaction-scale is retained
// here beyond the driver's bounded staging and the on-disk spools (the
// Phase A/B candidate spool, the base-stream replay spool, and the base
// cursor spool). The final posted book is handed off in the result for
// the AML exporters' account vertices.
//

#include "phantomledger/pipeline/stages/transfers/orchestrator.hpp"

#include "phantomledger/pipeline/acceptance/fingerprint.hpp"
#include "phantomledger/pipeline/diagnostics.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/pipeline/stages/transfers/binary_spool.hpp"
#include "phantomledger/pipeline/stages/transfers/replay_spool.hpp"
#include "phantomledger/pipeline/stages/transfers/window_sources.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/ledger/card_config.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/legit/routines/spending_session.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;
namespace legit_passes = legit_ledger::passes;
namespace routineSpending =
    ::PhantomLedger::transfers::legit::routines::spending;

using Txn = ::PhantomLedger::transactions::Transaction;

// Streams posted rows through account validation on their way to the
// caller's sink, matching runTransferStage's posted-corpus validation
// without retaining the corpus.
struct ValidatingSink {
  SinkRef inner;
  const entity::account::Lookup *lookup = nullptr;

  void beginSpan(const chunk::Span &span) { inner.beginSpan(span); }

  void append(std::span<const Txn> txns) {
    ::PhantomLedger::pipeline::validateTransactionAccounts(*lookup, txns);
    inner.append(txns);
  }

  void endSpan(const chunk::Span &span) { inner.endSpan(span); }

  void finish() { inner.finish(); }

  [[nodiscard]] std::uint64_t rowsWritten() const {
    return inner.rowsWritten();
  }
};

} // namespace

WindowedRunResult TransferStage::runWindowedErased(
    ::PhantomLedger::random::Rng &rng, const pipeline::People &people,
    pipeline::Holdings &holdings, const pipeline::Counterparties &cps,
    SinkRef sink, const WindowedRunOptions &options) const {
  legit_.validate();

  if (infra_ == nullptr) {
    throw std::runtime_error("transfers::TransferStage::runWindowed: infra "
                             "not set; call .infra(out.infra) first");
  }
  if (obligationSynthesis_ == nullptr) {
    throw std::runtime_error(
        "transfers::TransferStage::runWindowed: obligation synthesis not "
        "set; call .obligationSynthesis(pipeline products) first (RAM "
        "R2.2.1c: the product source replays it for the whole-window "
        "obligation stream)");
  }
  const auto &infra = *infra_;
  const auto scope = legit_.runScope();

  // Pristine copies for product and family generation (the
  // order-decoupling law): each relocatable generator routes on its own
  // copy of the pre-generation router state, so neither perturbs — nor is
  // perturbed by — the sticky state the session shares with income and
  // routines.
  ::PhantomLedger::infra::Router productRouter = productRouter_;
  ::PhantomLedger::infra::Router familyRouter = productRouter_;

  // 1. Generation prologue on the shared sequential stream.
  auto builder = legit_.builder(rng, legitWorldInputs(people, holdings, cps));
  builder.router(infra.router);
  auto prologue = builder.buildWindowedPrologue();

  if (prologue.initialBook == nullptr) {
    throw std::runtime_error("transfers::TransferStage::runWindowed: legit "
                             "prologue produced no initial book");
  }
  if (prologue.routinePass.txf() == nullptr) {
    throw std::runtime_error("transfers::TransferStage::runWindowed: the "
                             "windowed run requires a full account census "
                             "(base routines did not run)");
  }

  pipeline::diagnostics::logStageMem(
      "windowedPrologue",
      {{"screened", prologue.streams.screened().size()}});

  // 2. Persistent spending session over the screened base stream. Mirrors
  // passes::addSpending's preparation; the card-lifecycle config is the
  // exact shared constructor.
  const auto routineAccess = prologue.routinePass.accounts();
  const auto resources = prologue.routinePass.resources();

  if (resources.accountsLookup == nullptr) {
    throw std::invalid_argument("transfers::TransferStage::runWindowed: "
                                "spending requires a non-null accountsLookup");
  }

  const routineSpending::SpendingRoutine routine;
  const routineSpending::SpendingRoutine::CensusSource census{
      .blueprint = prologue.plan,
      .accounts =
          routineSpending::SpendingRoutine::AccountSource{
              .lookup = *resources.accountsLookup,
              .registry = *routineAccess.registry,
          },
  };

  const routineSpending::SpendingRoutine::PayeeDirectory payees{
      .merchants = resources.merchants,
      .creditCards = resources.creditCards,
  };

  const routineSpending::SpendingRoutine::ObligationSource obligationSource{
      .portfolios = resources.portfolios,
  };

  const std::span<const Txn> baseTxns(prologue.streams.screened());

  auto market = routine.prepareMarket(census, payees, baseTxns);
  // Mutable: the R2.4b-2 spool below rewires its replay feed BEFORE the
  // session's first advance (the planner reads the snapshot then).
  auto obligations = routineSpending::SpendingRoutine::prepareObligations(
      census, obligationSource, baseTxns, /*baseTxnsSorted=*/true);

  auto *screenBook = prologue.screen.fresh();
  if (screenBook == nullptr) {
    throw std::runtime_error(
        "transfers::TransferStage::runWindowed: screen book unavailable");
  }

  routineSpending::SessionInputs inputs;
  inputs.cardLifecycle =
      legit_passes::buildCardLifecycleConfig(prologue.plan, resources);
  if (options.threadCount.has_value()) {
    inputs.threadCount = options.threadCount;
  }

  const auto bundle = routineSpending::SessionBundle::make(
      prologue.plan.seed(), rng, *prologue.txf, market, obligations, screenBook,
      std::move(inputs));

  // RAM R2.4c.0: timeline probes — separate session construction and the
  // one-shot replay sort from the fold's own growth (the run peak now
  // accrues inside Phase A; windowed_driver.cpp carries the per-span
  // probe).
  pipeline::diagnostics::logStageMem("sessionPrepared", {});

  // 3. Window-independent cursor sources, precomputed at this fixed
  // sequence point. Products and family draw only from their dedicated
  // lanes and route from their pristine snapshots, so their generation
  // point cannot affect the session.
  const random::RngFactory rngFactory{scope.seed};

  // RAM R2.4b-2/3 (base cursor): the prologue built only the screened
  // view (deferReplayView); the replay order is derived ONCE here —
  // fundsLess (auditKey) is total for every output-affecting purpose
  // (S10: rows comparing equal are byte-identical), so this one-shot
  // sort equals the retired incremental merge. The sorted rows go
  // straight to disk through the candidate-spool machinery
  // (bit-identical records, audit-order verified by the cursor) and
  // the transient vector is freed; the fold reads one bounded buffer.
  BinaryCandidateSpool baseSpool;
  {
    auto replayRows = legit_ledger::sortForReplay(
        std::vector<Txn>(prologue.streams.screened().begin(),
                         prologue.streams.screened().end()));
    baseSpool.append(
        std::span<const Txn>(replayRows.data(), replayRows.size()));
    baseSpool.finish();
  } // replayRows freed here
  auto baseSource = baseSpool.openCursor();

  pipeline::diagnostics::logStageMem("baseSpooled", {});

  const transactions::Factory productTxf(rng, &productRouter,
                                         &infra.ringInfra);
  auto productSource = makeProductSource(
      scope.window, scope.seed, rngFactory, productTxf, people, holdings,
      *obligationSynthesis_, products_.insurancePrograms().claimRates);

  // RAM R2.2.1b/c: the world's obligation pack — since R2.2.1c just the
  // burden slice — was consumed by the burden prep during session
  // preparation, and the product source above derived its own transient
  // whole-window stream. Nothing later in the fold reads the pack, so
  // release it now. ObligationSynthesis::generateWindow() regenerates
  // any slice byte-identically on demand, and the finisher-time world
  // rebuild re-materializes the slice where a use case needs the world
  // back. Output is unaffected.
  holdings.portfolios.obligations() =
      ::PhantomLedger::entity::product::ObligationStream{};
  pipeline::diagnostics::logStageMem("obligationsReleased", {});

  // RAM R2.4b-2 (ledger replay): the spending prep has aggregated
  // everything it needs from the screened view (market paydays,
  // burdens), and its only remaining consumer is the day driver's
  // ledger replay — which now feeds from a sequential disk spool
  // through the replay seam (replay_source.hpp). Same rows, same
  // order, one bounded read buffer; the resident whole-window vector
  // is freed here.
  BaseReplaySpool replaySpool;
  replaySpool.spool(std::span<const Txn>(prologue.streams.screened()));
  replaySpool.seal();
  obligations.baseReplayOverride = &replaySpool;
  obligations.baseTxns = {};
  prologue.streams.releaseScreened();
  pipeline::diagnostics::logStageMem("screenedSpooled", {});

  auto familySource = std::make_unique<PrecomputedCursorSource>(
      legit_ledger::sortForReplay(
          builder.buildFamilyRows(prologue.plan, &familyRouter)));

  // 4. Two fresh opening-book copies: the pre-fraud and post-fraud folds
  // replay independently, exactly as in the monolithic path.
  auto preBook =
      std::make_unique<clearing::Ledger>(prologue.initialBook->clone());
  auto postBook =
      std::make_unique<clearing::Ledger>(prologue.initialBook->clone());

  // hashBook reads balances through Ledger's non-const accessors
  // (fingerprint.hpp) while the driver exposes the post book const-only;
  // keep a mutable handle, exactly as the gate harness does.
  auto *postBookPtr = postBook.get();

  WindowedConfig config;
  config.generation = options.generation;
  config.settlement = options.settlement;

  WindowedTransferDriver driver(std::move(preBook), std::move(postBook),
                                rngFactory, config);
  driver.session(bundle->session());
  driver.addCursorSource(*baseSource);
  driver.addCursorSource(*productSource);
  driver.addCursorSource(*familySource);

  // Fraud boundary inputs. The injector shares the sequential stream for
  // planning, exactly like the monolithic path; its budget denominator is
  // the exact realized candidate count delivered after Phase A.
  const auto injector = makeFraudInjector(rng, people, holdings);

  legit_ledger::LegitCounterparties legitCps;
  legitCps.hubAccounts = prologue.plan.counterparties().hubAccounts;
  legitCps.billerAccounts = prologue.plan.counterparties().billerAccounts;
  legitCps.employers = prologue.plan.counterparties().employers;

  const WindowedTransferDriver::FraudSourceFactory makeFraud =
      [&](std::uint64_t realizedCandidateCount)
      -> std::unique_ptr<ScheduleCursorSource> {
    return makeFraudSource(injector, scope.window,
                           static_cast<std::size_t>(realizedCandidateCount),
                           FraudEmission::legitCounterparties(legitCps));
  };

  ValidatingSink validating{sink, &holdings.accounts.lookup};

  WindowedRunResult out;

  if (options.binarySpool) {
    // The runTwoPhase composition with the file-backed spool at the
    // Phase A / Phase B boundary.
    BinaryCandidateSpool spool;
    out.summary.phaseA = driver.runPhaseA(scope.window, spool);

    pipeline::diagnostics::logStageMem(
        "phaseA",
        {{"candidate",
          static_cast<std::size_t>(out.summary.phaseA.candidateRows)}});

    const auto fraudSource = makeFraud(out.summary.phaseA.candidateRows);

    const auto candidates = spool.openCursor();
    out.summary.phaseB = driver.runPhaseB(scope.window, *candidates,
                                          fraudSource.get(), validating);

    out.spoolRows = spool.rowsWritten();
    out.spoolBytes = spool.bytesSpooled();

    pipeline::diagnostics::logStageMem(
        "phaseB",
        {{"posted", static_cast<std::size_t>(validating.rowsWritten())}});
  } else {
    out.summary = driver.runTwoPhase(scope.window, makeFraud, validating);

    pipeline::diagnostics::logStageMem(
        "twoPhase",
        {{"posted", static_cast<std::size_t>(validating.rowsWritten())}});
  }

  out.postedBookHash = acceptance::hashBook(*postBookPtr);

  // Hand the final book off to the caller (AML account vertices read its
  // balances); the fold is complete, so the driver never touches it again.
  out.postedBook = driver.takePostedBook();

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
