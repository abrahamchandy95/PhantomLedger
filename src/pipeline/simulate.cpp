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

[[nodiscard]] transferStage::TransferStage::RunScope
activeTransferScope(transferStage::TransferStage::RunScope requested,
                    ::PhantomLedger::time::Window fallbackWindow,
                    std::uint64_t fallbackSeed) noexcept {
  requested.window = activeTransferWindow(requested.window, fallbackWindow);
  requested.seed = activeTransferSeed(requested.seed, fallbackSeed);
  return requested;
}

} // namespace

SimulationPipeline::SimulationPipeline(::PhantomLedger::random::Rng &rng,
                                       ::PhantomLedger::time::Window window,
                                       entityStage::EntitySynthesis entities,
                                       std::uint64_t seed)
    : rng_(&rng), window_(window), seed_(seed), entities_(entities) {}

SimulationPipeline::SimulationPipeline(::PhantomLedger::random::Rng &rng,
                                       ::PhantomLedger::time::Window window,
                                       entityStage::EntitySynthesis entities,
                                       ObligationProducts obligationProducts,
                                       std::uint64_t seed)
    : rng_(&rng), window_(window), seed_(seed), entities_(entities),
      obligationProducts_(obligationProducts) {}

SimulationPipeline::TransferStage &
SimulationPipeline::transferStage() noexcept {
  return transfers_;
}

const SimulationPipeline::TransferStage &
SimulationPipeline::transferStage() const noexcept {
  return transfers_;
}

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
SimulationPipeline::obligationProducts(ObligationProducts value) noexcept {
  obligationProducts_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::productRng(productStage::PersonProductRng value) noexcept {
  obligationProducts_.rng = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::installmentLending(
    productStage::InstallmentLending value) noexcept {
  obligationProducts_.lending = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::taxWithholding(
    productStage::TaxWithholding value) noexcept {
  obligationProducts_.taxes = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::insuranceCoverage(
    productStage::InsuranceCoverage value) noexcept {
  obligationProducts_.coverage = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::transferScope(TransferStage::RunScope value) noexcept {
  transfers_.runScope(value);
  return *this;
}

SimulationPipeline &
SimulationPipeline::transferIncome(const TransferStage::IncomePrograms &value) {
  transfers_.incomePrograms(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::openingBalances(
    TransferStage::OpeningBalances value) noexcept {
  transfers_.openingBalances(value);
  return *this;
}

SimulationPipeline &
SimulationPipeline::cardLifecycle(TransferStage::CardLifecycle value) noexcept {
  transfers_.cardLifecycle(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::familyTransfers(
    transferStage::FamilyTransferScenario value) noexcept {
  transfers_.familyTransfers(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::insurancePrograms(
    TransferStage::InsurancePrograms value) noexcept {
  transfers_.insurancePrograms(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::replayOrdering(
    TransferStage::ReplayOrdering value) noexcept {
  transfers_.replayOrdering(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::fraudInjection(
    TransferStage::FraudInjection value) noexcept {
  transfers_.fraudInjection(value);
  return *this;
}

SimulationPipeline &
SimulationPipeline::hubSelection(TransferStage::HubSelection value) noexcept {
  transfers_.hubSelection(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::transferWindow(
    ::PhantomLedger::time::Window value) noexcept {
  transfers_.window(value);
  return *this;
}

SimulationPipeline &
SimulationPipeline::transferSeed(std::uint64_t value) noexcept {
  transfers_.seed(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::recurringIncome(
    const ::PhantomLedger::inflows::RecurringIncomeRules &value) {
  transfers_.recurringIncome(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::employmentRules(
    const ::PhantomLedger::recurring::EmploymentRules &value) {
  transfers_.employmentRules(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::leaseRules(
    const ::PhantomLedger::recurring::LeaseRules &value) {
  transfers_.leaseRules(value);
  return *this;
}

SimulationPipeline &
SimulationPipeline::salaryPaidFraction(double value) noexcept {
  transfers_.salaryPaidFraction(value);
  return *this;
}

SimulationPipeline &
SimulationPipeline::rentPaidFraction(double value) noexcept {
  transfers_.rentPaidFraction(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::openingBalanceRules(
    const ::PhantomLedger::clearing::BalanceRules *value) noexcept {
  transfers_.openingBalanceRules(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::creditLifecycle(
    const ::PhantomLedger::transfers::credit_cards::LifecycleRules
        *value) noexcept {
  transfers_.creditLifecycle(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::family(
    transferStage::FamilyTransferScenario value) noexcept {
  transfers_.family(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::retirementBenefits(
    const ::PhantomLedger::transfers::government::RetirementTerms &value) {
  transfers_.retirementBenefits(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::disabilityBenefits(
    const ::PhantomLedger::transfers::government::DisabilityTerms &value) {
  transfers_.disabilityBenefits(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::insuranceClaims(
    ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept {
  transfers_.insuranceClaims(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::replayRules(
    ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
        value) noexcept {
  transfers_.replayRules(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::fraudProfile(
    const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept {
  transfers_.fraudProfile(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::fraudRules(
    ::PhantomLedger::transfers::fraud::Injector::Rules value) noexcept {
  transfers_.fraudRules(value);
  return *this;
}

SimulationPipeline &SimulationPipeline::hubFraction(double value) noexcept {
  transfers_.hubFraction(value);
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
      out.entities.personas, out.entities.people, entities_.cards);

  out.entities.counterparties =
      entityStage::buildCounterparties(*rng_, entities_.people.population,
                                       entities_.counterparties.counterparties);

  // Register every external endpoint after entity construction and before
  // transfers so validation, balance bootstrap, standard export, and AML export
  // all see the same account registry.
  entityStage::finalizeAccountRegistry(out.entities);

  productStage::synthesize(out.entities, window_, obligationProducts_);

  out.infra = infra_.build(*rng_, out.entities, window_);

  auto configuredTransfers = transfers_;
  configuredTransfers.runScope(
      activeTransferScope(configuredTransfers.runScope(), window_, seed_));

  out.transfers = configuredTransfers.build(*rng_, out.entities, out.infra);

  return out;
}

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          ::PhantomLedger::time::Window window,
                          entityStage::EntitySynthesis entities,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, entities, seed}.run();
}

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          ::PhantomLedger::time::Window window,
                          entityStage::EntitySynthesis entities,
                          productStage::ObligationPrograms obligationProducts,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, entities, obligationProducts, seed}
      .run();
}

} // namespace PhantomLedger::pipeline
