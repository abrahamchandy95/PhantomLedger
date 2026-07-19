#include "phantomledger/pipeline/simulate.hpp"

#include "phantomledger/pipeline/chunk/schedule.hpp"

#include "phantomledger/pipeline/diagnostics.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

#include <cstdint>
#include <span>
#include <utility>

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = stages::entities;
namespace legit = ::PhantomLedger::transfers::legit;
namespace fraud = ::PhantomLedger::transfers::fraud;
namespace credit_cards = ::PhantomLedger::transfers::credit_cards;
namespace clearing = ::PhantomLedger::clearing;
namespace tx_ns = ::PhantomLedger::transactions;

using SynthFraud = ::PhantomLedger::synth::people::Fraud;

[[nodiscard]] auto resolveRunScope(legit::LegitAssembly::RunScope scope,
                                   time::Window fallbackWindow,
                                   std::uint64_t fallbackSeed) noexcept
    -> legit::LegitAssembly::RunScope {
  if (scope.window.days == 0) {
    scope.window = fallbackWindow;
  }
  if (scope.seed == 0) {
    scope.seed = fallbackSeed;
  }
  return scope;
}

void configureTransferStage(SimulationPipeline::TransferStage &stage,
                            time::Window window, std::uint64_t seed,
                            const SynthFraud &fraudProfile) noexcept {
  stage.legit().runScope(
      resolveRunScope(stage.legit().runScope(), window, seed));
  stage.legit().openingBalanceRules(&clearing::kDefaultBalanceRules);
  stage.legit().creditLifecycle(&credit_cards::kDefaultLifecycleRules);

  stage.fraud().profile(&fraudProfile);
  stage.fraud().behavior(&fraud::kDefaultBehavior);
}

void runTransferStage(SimulationResult &result,
                      SimulationPipeline::TransferStage &stage,
                      random::Rng &rng) {
  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &cps = result.counterparties;

  // Product generation and both settlement passes draw from dedicated
  // deterministic lanes derived from the run seed — the same lanes the
  // windowed driver owns — so both architectures share one RNG regime.
  // Legit generation and fraud planning stay on the shared sequential
  // stream. Corpus baseline is pinned in tests/golden_run.b2sum.
  const random::RngFactory laneFactory{stage.legit().runScope().seed};
  auto productRng = laneFactory.rng({"products", "full_schedule"});
  auto preSettleRng = laneFactory.rng({"settlement", "pre_fraud"});
  auto postSettleRng = laneFactory.rng({"settlement", "post_fraud"});

  auto legitPayload = stage.buildLegit(rng, people, holdings, cps);
  diagnostics::logStageMem(
      "buildLegit",
      {{"replaySorted", legitPayload.txns.replaySortedTxns.size()}});

  const auto replaySchedule = pipeline::chunk::Schedule::partition(
      stage.legit().runScope().window, stage.settlementChunking());
  auto productStream =
      stage.mergeProducts(productRng, holdings, std::move(legitPayload.txns));
  diagnostics::logStageMem("mergeProducts",
                           {{"productStream", productStream.size()}});
  auto candidate = stage.ledger().preFraudChunked(
      *legitPayload.openingBook.initialBook, preSettleRng,
      std::move(productStream), replaySchedule);
  diagnostics::logStageMem("preFraudSettle",
                           {{"candidate", candidate.txns.size()}});

  auto injector = stage.makeFraudInjector(rng, people, holdings);
  const std::span<const tx_ns::Transaction> candidateView{
      candidate.txns.data(), candidate.txns.size()};
  auto fraudOut =
      injector.inject(stage.legit().runScope().window, candidateView,
                      stages::transfers::FraudEmission::legitCounterparties(
                          legitPayload.counterparties));

  const auto injectedCount = fraudOut.injected.size();
  diagnostics::logStageMem("fraudInject",
                           {{"candidate", candidate.txns.size()},
                            {"fraud", fraudOut.injected.size()}});
  auto posted = stage.ledger().postFraudChunkedMerged(
      postSettleRng, *legitPayload.openingBook.initialBook,
      std::move(candidate.txns), std::move(fraudOut.injected), replaySchedule);
  diagnostics::logStageMem("postFraudSettle", {{"posted", posted.txns.size()}});

  validateTransactionAccounts(holdings.accounts.lookup, posted.txns);

  Transfers out{};
  out.legit = std::move(legitPayload);
  out.fraud.injectedCount = injectedCount;
  out.ledger.posted.txns = std::move(posted.txns);
  out.ledger.posted.book = std::move(posted.book);
  result.transfers = std::move(out);
}

} // namespace

SimulationPipeline::SimulationPipeline(random::Rng &rng, time::Window window,
                                       EntitySynthesis entities,
                                       std::uint64_t seed)
    : rng_(&rng), window_(window), seed_(seed), entities_(std::move(entities)) {
}

SimulationPipeline::InfraStage &SimulationPipeline::infraStage() noexcept {
  return infra_;
}

const SimulationPipeline::InfraStage &
SimulationPipeline::infraStage() const noexcept {
  return infra_;
}

SimulationPipeline::ProductSynthesis &SimulationPipeline::products() noexcept {
  return products_;
}

const SimulationPipeline::ProductSynthesis &
SimulationPipeline::products() const noexcept {
  return products_;
}

SimulationPipeline::TransferStage &
SimulationPipeline::transferStage() noexcept {
  return transfers_;
}

const SimulationPipeline::TransferStage &
SimulationPipeline::transferStage() const noexcept {
  return transfers_;
}

void SimulationPipeline::buildEntities(SimulationResult &result) const {
  const auto &cfg = entities_;
  const auto identity = entityStage::defaultStart(cfg.identity, window_.start);

  auto &people = result.people;
  auto &holdings = result.holdings;
  auto &cps = result.counterparties;

  people.roster = entityStage::buildPeople(*rng_, cfg.population, cfg.fraud);
  holdings.accounts = entityStage::buildAccounts(
      *rng_, people.roster, cfg.population, cfg.accountsSizing);
  people.personas =
      entityStage::buildPersonas(*rng_, people.roster, cfg.personaMix);
  people.pii = entityStage::buildPii(*rng_, people.personas, identity,
                                     people.roster.topology, cfg.piiSharing);

  cps.merchants =
      entityStage::buildMerchants(*rng_, cfg.population, cfg.merchants);
  cps.landlords =
      entityStage::buildLandlords(*rng_, cfg.population, cfg.landlords);
  cps.counterparties = entityStage::buildCounterparties(
      *rng_, cfg.population, cfg.counterpartyTargets);

  holdings.creditCards = entityStage::issueCreditCards(
      people.personas, people.roster, seed_, cfg.cards);

  entityStage::finalizeAccountRegistry(holdings, cps, people);
  entityStage::synthesizeBusinessOwners(holdings, people, *rng_,
                                        cfg.businessOwners);
}

SimulationResult
SimulationPipeline::buildWorld(const PhaseObserver &onPhase) const {
  SimulationResult out;

  const auto notify = [&](std::string_view phase) {
    if (onPhase) {
      onPhase(phase);
    }
  };

  buildEntities(out);
  notify("entities");
  diagnostics::logStageMem("worldEntities", {});

  products_.synthesize(out.people, out.holdings, window_);
  notify("products");
  diagnostics::logStageMem("worldProducts", {});

  out.infra = infra_.build(*rng_, out.people, out.holdings, window_);
  notify("infra");
  diagnostics::logStageMem("worldInfra", {});

  return out;
}

SimulationResult SimulationPipeline::run(const PhaseObserver &onPhase) const {
  auto out = buildWorld(onPhase);

  auto stage = transfers_;
  configureTransferStage(stage, window_, seed_, entities_.fraud);
  stage.infra(out.infra);

  runTransferStage(out, stage, *rng_);
  if (onPhase) {
    onPhase("transfers");
  }

  return out;
}

stages::transfers::WindowedRunResult
SimulationPipeline::runWindowedTransfersErased(
    SimulationResult &world, stages::transfers::SinkRef sink,
    const WindowedRunOptions &options, const PhaseObserver &onPhase) const {
  auto stage = transfers_;
  configureTransferStage(stage, window_, seed_, entities_.fraud);
  stage.infra(world.infra);

  auto out = stage.runWindowedErased(*rng_, world.people, world.holdings,
                                     world.counterparties, sink, options);
  if (onPhase) {
    onPhase("transfers");
  }
  return out;
}

WindowedSimulationResult SimulationPipeline::runWindowedErased(
    stages::transfers::SinkRef sink, const WindowedRunOptions &options,
    const PhaseObserver &onPhase) const {
  WindowedSimulationResult out;

  // Identical world build to run(): same stages, same shared-stream
  // consumption, so the transfer fold starts from a byte-identical world.
  out.world = buildWorld(onPhase);
  out.transfers = runWindowedTransfersErased(out.world, sink, options, onPhase);

  return out;
}

SimulationResult simulate(random::Rng &rng, time::Window window,
                          SimulationPipeline::EntitySynthesis entities,
                          std::uint64_t seed, const PhaseObserver &onPhase) {
  return SimulationPipeline{rng, window, std::move(entities), seed}.run(
      onPhase);
}

} // namespace PhantomLedger::pipeline
