#include "phantomledger/pipeline/simulate.hpp"

#include "phantomledger/pipeline/chunk/schedule.hpp"

#include "phantomledger/pipeline/diagnostics.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/pipeline/world_footprint.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

#include <algorithm>
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

  /* Product generation and both settlement passes draw from dedicated
   * deterministic lanes derived from the run seed — the same lanes the
   * windowed driver owns — so both architectures share one RNG regime. Legit
   * generation and fraud planning stay on the shared sequential stream. The
   * corpus baseline is pinned in tests/golden_run.b2sum. */
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
  auto productStream = stage.mergeProducts(productRng, people, holdings,
                                           std::move(legitPayload.txns));
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
  /* The merchant acceptance catalogue and the home-area axis ride along with
   * the legit pools. THE WINDOWED ENGINE (windowed_run.cpp) MUST PASS THE SAME
   * ARGUMENTS — this is the reference oracle test_arch_equivalence compares
   * against, so any asymmetry here becomes an engine divergence. */
  auto fraudOut = injector.inject(
      stage.legit().runScope().window, candidateView,
      stages::transfers::FraudEmission::legitCounterparties(
          legitPayload.counterparties, &cps.merchants, people.homeAreas,
          &people.personas, &people.relocation));

  const auto activeStart =
      time::toEpochSeconds(stage.legit().runScope().window.start);
  const auto activeEnd =
      time::toEpochSeconds(stage.legit().runScope().window.endExcl());
  const auto injectedCount = static_cast<std::size_t>(std::count_if(
      fraudOut.injected.begin(), fraudOut.injected.end(),
      [activeStart, activeEnd](const tx_ns::Transaction &txn) {
        return txn.timestamp >= activeStart && txn.timestamp < activeEnd;
      }));
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

void SimulationPipeline::buildEntities(SimulationResult &result,
                                       random::Rng &rng) const {
  const auto &cfg = entities_;
  auto identity = entityStage::defaultStart(cfg.identity, window_.start);
  /* The production pipeline always sizes the join cohort against its own
   * window (Pack::joinDays; authority U-8 addendum). Direct entity-stage
   * callers that leave windowDays 0 build no cohort. */
  identity.windowDays = window_.days;

  auto &people = result.people;
  auto &holdings = result.holdings;
  auto &cps = result.counterparties;

  people.roster = entityStage::buildPeople(rng, cfg.population, cfg.fraud);
  holdings.accounts = entityStage::buildAccounts(
      rng, people.roster, cfg.population, cfg.accountsSizing);
  /* The personas pack carries the single-age-axis birth dates
   * ({"dob", personId} lanes off identity.worldSeed) and the join cohort;
   * a joiner's age anchors at their JOIN date. PII below renders its Dob from
   * the carrier. */
  people.personas =
      entityStage::buildPersonas(rng, people.roster, identity, cfg.personaMix);
  people.pii = entityStage::buildPii(rng, people.personas, identity,
                                     people.roster.topology, cfg.piiSharing);

  /* Snapshot the compact home-area carrier NOW, while PII is alive:
   * releaseExportOnlyPacks() nulls people.pii before the transfer fold, and
   * causal card-present selection needs the home area. The fraud injector
   * reads the SAME carrier, so fraud and legitimate card activity share one
   * geographic axis. */
  people.homeAreas = homeAreasOf(people.pii);

  /* The home-area HISTORY, built here for the same reason the snapshot above
   * is: PII is alive and carries the per-person country the destination
   * constraint needs. Runs on its OWN `{"home-relocation", <group>}` lane, so
   * it spends nothing on `rng` and cannot move a downstream entity value. */
  people.relocation = entityStage::buildRelocation(
      people.pii, people.homeAreas, people.personas, seed_, window_);

  /* seed_ (the run seed) drives ONLY the merchant footprint/location lanes,
   * which are isolated from `rng`; the merchant catalogue's economic draws
   * still come off the shared stream, so the corpus is byte-identical. */
  cps.merchants =
      entityStage::buildMerchants(rng, cfg.population, seed_, window_,
                                  cfg.merchants, &synth::econ::macroSeries());
  cps.landlords =
      entityStage::buildLandlords(rng, cfg.population, cfg.landlords);
  cps.counterparties = entityStage::buildCounterparties(
      rng, cfg.population, cfg.counterpartyTargets);

  /* Credit limits are a class P STOCK: they anchor at the window-start
   * year's price level (authority U-6). */
  holdings.creditCards = entityStage::issueCreditCards(
      people.personas, people.roster, seed_, cfg.cards,
      time::toCalendarDate(window_.start).year);

  entityStage::finalizeAccountRegistry(holdings, cps, people);
  entityStage::synthesizeBusinessOwners(holdings, people, rng,
                                        cfg.businessOwners);

  /* Stamp the beneficial-owner register onto the catalogue. MUST run AFTER
   * synthesizeBusinessOwners — the proprietor cohort is the business-owner
   * cohort — and it is DRAW-FREE, so it appends nothing to `rng` and the
   * corpus stream does not move. Exported as `cf_Is_Merchant`, whose emptiness
   * hard-aborts the downstream tf_gnn_loader_v2 push before any data reaches
   * TigerGraph. */
  entityStage::assignMerchantOwners(cps.merchants, holdings.accounts.registry);
}

SimulationResult
SimulationPipeline::buildWorldWith(random::Rng &rng,
                                   const PhaseObserver &onPhase) const {
  SimulationResult out;

  const auto notify = [&](std::string_view phase) {
    if (onPhase) {
      onPhase(phase);
    }
  };

  buildEntities(out, rng);
  notify("entities");
  diagnostics::logStageMem("worldEntities", {});

  products_.synthesize(out.people, out.holdings, window_);
  notify("products");
  diagnostics::logStageMem("worldProducts", {});

  out.infra = infra_.build(rng, out.people, out.holdings, window_, seed_);
  notify("infra");
  diagnostics::logStageMem("worldInfra", {});

  /* Which packs hold the world's resident bytes, plus the obligation burden
   * slice's pinned-order audit. Prints only under the `mem` topic. */
  diagnostics::logWorldFootprint(out.people, out.holdings, out.counterparties,
                                 out.infra);
  diagnostics::logObligationTieAudit(out.holdings.portfolios, window_);

  return out;
}

SimulationResult
SimulationPipeline::buildWorld(const PhaseObserver &onPhase) const {
  return buildWorldWith(*rng_, onPhase);
}

SimulationResult
SimulationPipeline::rebuildWorldForExport(const PhaseObserver &onPhase) const {
  /* Fresh generator at the run seed: world-build draws are a prefix of the
   * shared sequential stream, so this replay is byte-identical to the original
   * buildWorld(). The shared RNG's later position — after the transfer fold —
   * is irrelevant here and stays untouched. */
  auto replayRng = random::Rng::fromSeed(seed_);
  return buildWorldWith(replayRng, onPhase);
}

void releaseExportOnlyPacks(SimulationResult &world) noexcept {
  /* Move-assign empty packs: releases the old storage now. The Router keeps
   * its own copies of the per-person device/IP pools, so routing is
   * unaffected; the fold reads none of these (their only consumers are the
   * vertex exporters). people.homeAreas is a SEPARATE compact carrier that
   * must OUTLIVE this release — the fold's causal selection and the fraud
   * injector both read it — so it is deliberately NOT cleared here. The
   * personas pack, with its birth-date carrier, also survives: the blueprint
   * and the government pass read it inside the fold. */
  world.people.pii = entity::pii::Roster{};
  world.infra.devices = synth::infra::devices::Output{};
  world.infra.ips = synth::infra::ips::Output{};
}

SimulationResult SimulationPipeline::run(const PhaseObserver &onPhase) const {
  auto out = buildWorld(onPhase);

  auto stage = transfers_;
  configureTransferStage(stage, window_, seed_, entities_.fraud);
  stage.infra(out.infra);
  /* The product emitters replay THIS synthesis — the exact config that built
   * the world's portfolio terms — for the transient whole-window obligation
   * stream. */
  stage.obligationSynthesis(products_);

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
  stage.obligationSynthesis(products_);

  auto out = stage.runWindowedErased(*rng_, world.people, world.holdings,
                                     world.counterparties, sink, options);
  if (onPhase) {
    onPhase("transfers");
  }
  return out;
}

WindowedSimulationResult
SimulationPipeline::runWindowedErased(stages::transfers::SinkRef sink,
                                      const WindowedRunOptions &options,
                                      const PhaseObserver &onPhase) const {
  WindowedSimulationResult out;

  /* Identical world build to run(): same stages, same shared-stream
   * consumption, so the transfer fold starts from a byte-identical world. */
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
