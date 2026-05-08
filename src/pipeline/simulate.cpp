#include "phantomledger/pipeline/simulate.hpp"

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = ::PhantomLedger::pipeline::stages::entities;
namespace transferStage = ::PhantomLedger::pipeline::stages::transfers;

[[nodiscard]] ::PhantomLedger::time::Window
activeTransferWindow(::PhantomLedger::time::Window requested,
                     ::PhantomLedger::time::Window fallback) noexcept {
  if (requested.days == 0) {
    return fallback;
  }

  return requested;
}

[[nodiscard]] std::uint64_t
activeTransferSeed(std::uint64_t requested, std::uint64_t fallback) noexcept {
  if (requested == 0) {
    return fallback;
  }

  return requested;
}

[[nodiscard]] transferStage::LegitAssembly::RunScope
activeTransferScope(transferStage::LegitAssembly::RunScope requested,
                    ::PhantomLedger::time::Window fallbackWindow,
                    std::uint64_t fallbackSeed) noexcept {
  requested.window = activeTransferWindow(requested.window, fallbackWindow);
  requested.seed = activeTransferSeed(requested.seed, fallbackSeed);
  return requested;
}

} // namespace

SimulationPipeline::SimulationPipeline(::PhantomLedger::random::Rng &rng,
                                       ::PhantomLedger::time::Window window,
                                       EntitySynthesis entities,
                                       std::uint64_t seed)
    : rng_(&rng), window_(window), seed_(seed), entities_(entities) {}

SimulationPipeline::SimulationPipeline(::PhantomLedger::random::Rng &rng,
                                       ::PhantomLedger::time::Window window,
                                       EntitySynthesis entities,
                                       ProductSynthesis products,
                                       std::uint64_t seed)
    : rng_(&rng), window_(window), seed_(seed), entities_(entities),
      products_(products) {}

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

  const auto identity =
      entityStage::defaultStart(entities_.identity, window_.start);

  out.entities.people =
      entityStage::buildPeople(*rng_, entities_.population, entities_.fraud);

  out.entities.accounts = entityStage::buildAccounts(*rng_, out.entities.people,
                                                     entities_.population,
                                                     entities_.accountsSizing);

  out.entities.personas = entityStage::buildPersonas(*rng_, out.entities.people,
                                                     entities_.personaMix);

  out.entities.pii =
      entityStage::buildPii(*rng_, out.entities.personas, identity);

  // buildMerchants returns entity::merchant::Catalog directly.
  out.entities.merchants = entityStage::buildMerchants(
      *rng_, entities_.population, entities_.merchants);

  out.entities.landlords = entityStage::buildLandlords(
      *rng_, entities_.population, entities_.landlords);

  // issueCreditCards takes the user's top-level seed and derives the
  // card-subsystem subseed via deriveCardSeed (cards/seeds.hpp).
  out.entities.creditCards = entityStage::issueCreditCards(
      out.entities.personas, out.entities.people, seed_, entities_.cards);

  out.entities.counterparties = entityStage::buildCounterparties(
      *rng_, entities_.population, entities_.counterpartyTargets);

  // Register every external endpoint after entity construction and before
  // transfers so validation, balance bootstrap, standard export, and AML
  // export all see the same account registry.
  entityStage::finalizeAccountRegistry(out.entities);

  products_.synthesize(out.entities, window_);

  out.infra = infra_.build(*rng_, out.entities, window_);

  auto configuredTransfers = transfers_;
  configuredTransfers.legit().runScope(activeTransferScope(
      configuredTransfers.legit().runScope(), window_, seed_));

  out.transfers = configuredTransfers.build(*rng_, out.entities, out.infra);

  return out;
}

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          ::PhantomLedger::time::Window window,
                          SimulationPipeline::EntitySynthesis entities,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, entities, seed}.run();
}

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          ::PhantomLedger::time::Window window,
                          SimulationPipeline::EntitySynthesis entities,
                          SimulationPipeline::ProductSynthesis products,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, entities, products, seed}.run();
}

} // namespace PhantomLedger::pipeline
