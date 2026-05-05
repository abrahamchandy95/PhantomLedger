#include "phantomledger/pipeline/simulate.hpp"

#include "phantomledger/pipeline/stages/products.hpp"

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = ::PhantomLedger::pipeline::stages::entities;
namespace infraStage = ::PhantomLedger::pipeline::stages::infra;
namespace productStage = ::PhantomLedger::pipeline::stages::products;
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

SimulationPipeline &SimulationPipeline::transferWindow(
    ::PhantomLedger::time::Window value) noexcept {
  transferWindow_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::transferSeed(std::uint64_t value) noexcept {
  transferSeed_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::recurringIncome(
    const ::PhantomLedger::inflows::RecurringIncomeRules &value) {
  recurringIncome_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::employmentRules(
    const ::PhantomLedger::recurring::EmploymentRules &value) {
  recurringIncome_.employment = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::leaseRules(
    const ::PhantomLedger::recurring::LeaseRules &value) {
  recurringIncome_.lease = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::salaryPaidFraction(double value) noexcept {
  recurringIncome_.salaryPaidFraction = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::rentPaidFraction(double value) noexcept {
  recurringIncome_.rentPaidFraction = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::openingBalanceRules(
    const ::PhantomLedger::clearing::BalanceRules *value) noexcept {
  openingBalanceRules_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::creditLifecycle(
    const ::PhantomLedger::transfers::credit_cards::LifecycleRules
        *value) noexcept {
  creditLifecycle_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::family(
    transferStage::FamilyTransferScenario value) noexcept {
  familyScenario_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::retirementBenefits(
    const ::PhantomLedger::transfers::government::RetirementTerms &value) {
  retirement_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::disabilityBenefits(
    const ::PhantomLedger::transfers::government::DisabilityTerms &value) {
  disability_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::insuranceClaims(
    ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept {
  claimRates_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::replayRules(
    ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
        value) noexcept {
  replayRules_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::fraudProfile(
    const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept {
  fraudProfile_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::fraudRules(
    ::PhantomLedger::transfers::fraud::Injector::Rules value) noexcept {
  fraudRules_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::hubFraction(double value) noexcept {
  hubFraction_ = value;
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
  transfers.window(activeTransferWindow(transferWindow_, window_))
      .seed(activeTransferSeed(transferSeed_, seed_))
      .recurringIncome(recurringIncome_)
      .openingBalanceRules(openingBalanceRules_)
      .creditLifecycle(creditLifecycle_)
      .family(familyScenario_)
      .retirementBenefits(retirement_)
      .disabilityBenefits(disability_)
      .insuranceClaims(claimRates_)
      .replayRules(replayRules_)
      .fraudProfile(fraudProfile_)
      .fraudRules(fraudRules_)
      .hubFraction(hubFraction_);

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
