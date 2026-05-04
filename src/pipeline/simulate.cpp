#include "phantomledger/pipeline/simulate.hpp"

#include "phantomledger/pipeline/stages/products.hpp"

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = ::PhantomLedger::pipeline::stages::entities;
namespace infraStage = ::PhantomLedger::pipeline::stages::infra;
namespace productStage = ::PhantomLedger::pipeline::stages::products;
namespace transferStage = ::PhantomLedger::pipeline::stages::transfers;

[[nodiscard]] transferStage::RunScope
activeTransferScope(transferStage::RunScope requested,
                    ::PhantomLedger::time::Window fallbackWindow,
                    std::uint64_t fallbackSeed) noexcept {
  if (requested.window.days == 0) {
    requested.window = fallbackWindow;
  }

  if (requested.seed == 0) {
    requested.seed = fallbackSeed;
  }

  return requested;
}

} // namespace

SimulationPipeline::SimulationPipeline(::PhantomLedger::random::Rng &rng,
                                       ::PhantomLedger::time::Window window,
                                       entityStage::EntitySynthesis entities,
                                       std::uint64_t seed)
    : rng_(&rng), window_(window), seed_(seed), entities_(entities) {}

SimulationPipeline &
SimulationPipeline::infraWindow(::PhantomLedger::time::Window value) noexcept {
  infra_.window(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::ringAccess(
    const ::PhantomLedger::infra::synth::rings::AccessRules &value) noexcept {
  infra_.ringAccess(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::deviceAssignment(
    const ::PhantomLedger::infra::synth::devices::AssignmentRules
        &value) noexcept {
  infra_.deviceAssignment(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::ipAssignment(
    const ::PhantomLedger::infra::synth::ips::AssignmentRules &value) noexcept {
  infra_.ipAssignment(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::routingBehavior(
    infraStage::RoutingBehavior value) noexcept {
  infra_.routingBehavior(value);
  return *this;
}

SimulationPipeline &
SimulationPipeline::sharedInfra(infraStage::SharedInfraUse value) noexcept {
  infra_.sharedInfra(value);
  return *this;
}

SimulationPipeline &
SimulationPipeline::transferScope(transferStage::RunScope value) noexcept {
  transferScope_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::recurringIncome(
    const transferStage::RecurringIncome &value) {
  recurringIncome_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::balanceBook(
    transferStage::BalanceBookRules value) noexcept {
  balanceBook_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::creditCards(
    transferStage::CreditCardLifecycle value) noexcept {
  creditCards_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::family(transferStage::FamilyPrograms value) noexcept {
  family_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::government(const transferStage::GovernmentPrograms &value) {
  government_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::insurance(transferStage::InsuranceClaims value) noexcept {
  insurance_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::replay(transferStage::LedgerReplay value) noexcept {
  replay_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::fraud(transferStage::FraudInjection value) noexcept {
  fraud_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::population(transferStage::PopulationShape value) noexcept {
  population_ = value;
  return *this;
}

SimulationResult SimulationPipeline::run() const {
  SimulationResult out;

  entityStage::validate(entities_.people.population);

  const auto identity =
      entityStage::withDefaultStart(entities_.people.identity, window_.start);

  out.entities.people = entityStage::buildPeople(
      *rng_, entities_.people.population, entities_.people.fraud);

  out.entities.accounts = entityStage::buildAccounts(
      *rng_, out.entities.people, entities_.people.population);

  out.entities.personas = entityStage::buildPersonas(
      *rng_, out.entities.people, entities_.people.personaMix);

  out.entities.pii =
      entityStage::buildPii(*rng_, out.entities.personas, identity);

  out.entities.merchants = entityStage::buildMerchants(
      *rng_, entities_.people.population, entities_.counterparties.merchants);

  out.entities.landlords = entityStage::buildLandlords(
      *rng_, entities_.people.population, entities_.counterparties.landlords);

  out.entities.creditCards = entityStage::issueCreditCards(
      out.entities.personas, out.entities.people, entities_.products.seeds,
      entities_.products.cardIssuance);

  out.entities.counterparties =
      entityStage::buildCounterparties(*rng_, entities_.people.population,
                                       entities_.counterparties.counterparties);

  // Register every external endpoint after entity construction and before
  // transfers so validation, balance bootstrap, standard export, and AML export
  // all see the same account registry.
  entityStage::finalizeAccountRegistry(out.entities);

  productStage::synthesize(out.entities, window_, entities_.products);

  out.infra = infra_.build(*rng_, out.entities, window_);

  transferStage::TransferStage transfers{*rng_, out.entities, out.infra};
  transfers.scope(activeTransferScope(transferScope_, window_, seed_))
      .income(recurringIncome_)
      .balanceBook(balanceBook_)
      .creditCards(creditCards_)
      .family(family_)
      .government(government_)
      .insurance(insurance_)
      .replay(replay_)
      .fraud(fraud_)
      .population(population_);

  out.transfers = transfers.build();

  return out;
}

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          ::PhantomLedger::time::Window window,
                          entityStage::EntitySynthesis entities,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, entities, seed}.run();
}

} // namespace PhantomLedger::pipeline
