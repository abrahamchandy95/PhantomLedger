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
  transferScope_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::transferIncome(const TransferStage::IncomePrograms &value) {
  transferIncome_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::openingBalances(
    TransferStage::OpeningBalances value) noexcept {
  openingBalances_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::cardLifecycle(TransferStage::CardLifecycle value) noexcept {
  cardLifecycle_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::familyTransfers(
    transferStage::FamilyTransferScenario value) noexcept {
  familyTransfers_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::insurancePrograms(
    TransferStage::InsurancePrograms value) noexcept {
  insurancePrograms_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::replayOrdering(
    TransferStage::ReplayOrdering value) noexcept {
  replayOrdering_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::fraudInjection(
    TransferStage::FraudInjection value) noexcept {
  fraudInjection_ = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::hubSelection(TransferStage::HubSelection value) noexcept {
  hubSelection_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::transferWindow(
    ::PhantomLedger::time::Window value) noexcept {
  transferScope_.window = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::transferSeed(std::uint64_t value) noexcept {
  transferScope_.seed = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::recurringIncome(
    const ::PhantomLedger::inflows::RecurringIncomeRules &value) {
  transferIncome_.recurring = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::employmentRules(
    const ::PhantomLedger::recurring::EmploymentRules &value) {
  transferIncome_.recurring.employment = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::leaseRules(
    const ::PhantomLedger::recurring::LeaseRules &value) {
  transferIncome_.recurring.lease = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::salaryPaidFraction(double value) noexcept {
  transferIncome_.recurring.salaryPaidFraction = value;
  return *this;
}

SimulationPipeline &
SimulationPipeline::rentPaidFraction(double value) noexcept {
  transferIncome_.recurring.rentPaidFraction = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::openingBalanceRules(
    const ::PhantomLedger::clearing::BalanceRules *value) noexcept {
  openingBalances_.balanceRules = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::creditLifecycle(
    const ::PhantomLedger::transfers::credit_cards::LifecycleRules
        *value) noexcept {
  cardLifecycle_.lifecycleRules = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::family(
    transferStage::FamilyTransferScenario value) noexcept {
  familyTransfers_ = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::retirementBenefits(
    const ::PhantomLedger::transfers::government::RetirementTerms &value) {
  transferIncome_.retirement = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::disabilityBenefits(
    const ::PhantomLedger::transfers::government::DisabilityTerms &value) {
  transferIncome_.disability = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::insuranceClaims(
    ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept {
  insurancePrograms_.claimRates = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::replayRules(
    ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
        value) noexcept {
  replayOrdering_.replayRules = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::fraudProfile(
    const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept {
  fraudInjection_.profile = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::fraudRules(
    ::PhantomLedger::transfers::fraud::Injector::Rules value) noexcept {
  fraudInjection_.injectorRules = value;
  return *this;
}

SimulationPipeline &SimulationPipeline::hubFraction(double value) noexcept {
  hubSelection_.fraction = value;
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

  transferStage::TransferStage transfers{*rng_, out.entities, out.infra};
  transfers.runScope(activeTransferScope(transferScope_, window_, seed_))
      .incomePrograms(transferIncome_)
      .openingBalances(openingBalances_)
      .cardLifecycle(cardLifecycle_)
      .familyTransfers(familyTransfers_)
      .insurancePrograms(insurancePrograms_)
      .replayOrdering(replayOrdering_)
      .fraudInjection(fraudInjection_)
      .hubSelection(hubSelection_);

  out.transfers = transfers.build();

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
