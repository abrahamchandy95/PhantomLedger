#include "phantomledger/pipeline/simulate.hpp"

#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

#include <cstdint>
#include <utility>

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = stages::entities;
namespace legit = ::PhantomLedger::transfers::legit;
namespace fraud = ::PhantomLedger::transfers::fraud;
namespace credit_cards = ::PhantomLedger::transfers::credit_cards;
namespace clearing = ::PhantomLedger::clearing;

using SynthFraud = ::PhantomLedger::entities::synth::people::Fraud;

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

// Entity construction

[[nodiscard]] Entities
buildEntities(random::Rng &rng, time::Window window,
              const SimulationPipeline::EntitySynthesis &cfg,
              std::uint64_t seed) {
  const auto identity = entityStage::defaultStart(cfg.identity, window.start);

  Entities out;
  out.people = entityStage::buildPeople(rng, cfg.population, cfg.fraud);
  out.accounts = entityStage::buildAccounts(rng, out.people, cfg.population,
                                            cfg.accountsSizing);
  out.personas = entityStage::buildPersonas(rng, out.people, cfg.personaMix);
  out.pii = entityStage::buildPii(rng, out.personas, identity);
  out.merchants =
      entityStage::buildMerchants(rng, cfg.population, cfg.merchants);
  out.landlords =
      entityStage::buildLandlords(rng, cfg.population, cfg.landlords);
  out.creditCards =
      entityStage::issueCreditCards(out.personas, out.people, seed, cfg.cards);
  out.counterparties = entityStage::buildCounterparties(
      rng, cfg.population, cfg.counterpartyTargets);

  entityStage::finalizeAccountRegistry(out);
  entityStage::synthesizeBusinessOwners(out, rng, cfg.businessOwners);
  return out;
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

SimulationResult SimulationPipeline::run() const {
  SimulationResult out;

  out.entities = buildEntities(*rng_, window_, entities_, seed_);
  products_.synthesize(out.entities, window_);
  out.infra = infra_.build(*rng_, out.entities, window_);

  auto stage = transfers_;
  configureTransferStage(stage, window_, seed_, entities_.fraud);
  out.transfers = stage.build(*rng_, out.entities, out.infra);

  return out;
}

SimulationResult simulate(random::Rng &rng, time::Window window,
                          SimulationPipeline::EntitySynthesis entities,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, std::move(entities), seed}.run();
}

} // namespace PhantomLedger::pipeline
