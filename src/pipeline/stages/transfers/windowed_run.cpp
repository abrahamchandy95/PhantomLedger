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
//   3. cursor sources        base (income+routines), products
//                            (products/full_schedule lane, pristine
//                            router copy), family (family lanes,
//                            pristine router copy)
//   4. two-phase fold        Phase A -> candidate spool -> exact count L
//                            -> fraud source -> Phase B -> sink
//
// Rows stream to the caller's sink; nothing transaction-scale is retained
// here beyond the driver's bounded staging and (optionally) the on-disk
// candidate spool. The final posted book is handed off in the result for
// the AML exporters' account vertices.
//

#include "phantomledger/pipeline/stages/transfers/orchestrator.hpp"

#include "phantomledger/pipeline/acceptance/fingerprint.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/pipeline/stages/transfers/binary_spool.hpp"
#include "phantomledger/pipeline/stages/transfers/window_sources.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/ledger/card_config.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/legit/routines/spending_session.hpp"

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
    const pipeline::Holdings &holdings, const pipeline::Counterparties &cps,
    SinkRef sink, const WindowedRunOptions &options) const {
  legit_.validate();

  if (infra_ == nullptr) {
    throw std::runtime_error("transfers::TransferStage::runWindowed: infra "
                             "not set; call .infra(out.infra) first");
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
  auto builder = legit_.builder(rng, people, holdings, cps);
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
  const auto obligations = routineSpending::SpendingRoutine::prepareObligations(
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

  // 3. Window-independent cursor sources, precomputed at this fixed
  // sequence point. Products and family draw only from their dedicated
  // lanes and route from their pristine snapshots, so their generation
  // point cannot affect the session.
  const random::RngFactory rngFactory{scope.seed};

  auto baseSource = std::make_unique<PrecomputedCursorSource>(
      prologue.streams.takeReplayReady());

  const auto primaryByPerson = primaryAccounts(holdings);
  const transactions::Factory productTxf(rng, &productRouter,
                                         &infra.ringInfra);
  auto productSource = makeProductSource(
      scope.window, scope.seed, rngFactory, productTxf, holdings,
      products_.insurancePrograms().claimRates, primaryByPerson);

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

    const auto fraudSource = makeFraud(out.summary.phaseA.candidateRows);

    const auto candidates = spool.openCursor();
    out.summary.phaseB = driver.runPhaseB(scope.window, *candidates,
                                          fraudSource.get(), validating);

    out.spoolRows = spool.rowsWritten();
    out.spoolBytes = spool.bytesSpooled();
  } else {
    out.summary = driver.runTwoPhase(scope.window, makeFraud, validating);
  }

  out.postedBookHash = acceptance::hashBook(*postBookPtr);

  // Hand the final book off to the caller (AML account vertices read its
  // balances); the fold is complete, so the driver never touches it again.
  out.postedBook = driver.takePostedBook();

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
