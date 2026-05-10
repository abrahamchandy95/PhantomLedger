#include "phantomledger/pipeline/simulate.hpp"

#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = ::PhantomLedger::pipeline::stages::entities;
namespace legit = ::PhantomLedger::transfers::legit;

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

[[nodiscard]] legit::LegitAssembly::RunScope
activeTransferScope(legit::LegitAssembly::RunScope requested,
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

  out.entities.merchants = entityStage::buildMerchants(
      *rng_, entities_.population, entities_.merchants);

  out.entities.landlords = entityStage::buildLandlords(
      *rng_, entities_.population, entities_.landlords);

  out.entities.creditCards = entityStage::issueCreditCards(
      out.entities.personas, out.entities.people, seed_, entities_.cards);

  out.entities.counterparties = entityStage::buildCounterparties(
      *rng_, entities_.population, entities_.counterpartyTargets);

  entityStage::finalizeAccountRegistry(out.entities);

  products_.synthesize(out.entities, window_);

  out.infra = infra_.build(*rng_, out.entities, window_);

  auto configuredTransfers = transfers_;
  configuredTransfers.legit().runScope(activeTransferScope(
      configuredTransfers.legit().runScope(), window_, seed_));
  configuredTransfers.legit().creditLifecycle(
      &::PhantomLedger::transfers::credit_cards::kDefaultLifecycleRules);
  configuredTransfers.legit().openingBalanceRules(
      &::PhantomLedger::clearing::kDefaultBalanceRules);
  out.transfers = configuredTransfers.build(*rng_, out.entities, out.infra);

  return out;
}

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          ::PhantomLedger::time::Window window,
                          SimulationPipeline::EntitySynthesis entities,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, entities, seed}.run();
}

} // namespace PhantomLedger::pipeline
