#include "phantomledger/pipeline/simulate.hpp"

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = ::PhantomLedger::pipeline::stages::entities;
namespace productSynth = ::PhantomLedger::entities::synth::products;

void synthesizeProducts(SimulationResult &out,
                        ::PhantomLedger::time::Window window,
                        const SimulateEntities &entities) {
  const auto &assignment = out.entities.personas.assignment;
  const auto population = static_cast<::PhantomLedger::entity::PersonId>(
      assignment.byPerson.size());

  for (::PhantomLedger::entity::PersonId person = 1; person <= population;
       ++person) {
    auto local = productSynth::personRng(entities.seeds.products, person);
    const auto persona = assignment.byPerson[person - 1];

    const double mortgageAnchorP =
        entities.mortgage.adoption.probability(persona);
    const double autoLoanAnchorP =
        entities.autoLoan.adoption.probability(persona);

    const bool hasMortgage =
        productSynth::emitMortgage(local, out.entities.portfolios, person,
                                   persona, window, entities.mortgage);

    const bool hasAutoLoan =
        productSynth::emitAutoLoan(local, out.entities.portfolios, person,
                                   persona, window, entities.autoLoan);

    (void)productSynth::emitStudentLoan(local, out.entities.portfolios, person,
                                        persona, window, entities.studentLoan);

    (void)productSynth::emitTax(local, out.entities.portfolios, person, persona,
                                window, entities.tax);

    (void)productSynth::emitInsurance(
        local, out.entities.portfolios, person, persona, hasMortgage,
        hasAutoLoan, mortgageAnchorP, autoLoanAnchorP, entities.insurance);
  }

  out.entities.portfolios.obligations().sort();
}

} // namespace

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          const SimulateInputs &in) {
  SimulationResult out;

  entityStage::validate(in.entities.population);

  const auto identity =
      entityStage::withDefaultStart(in.entities.identity, in.window.start);

  auto infraIn = in.infraIn;
  if (infraIn.window.days == 0) {
    infraIn.window = in.window;
  }

  out.entities.people =
      entityStage::buildPeople(rng, in.entities.population, in.entities.fraud);

  out.entities.accounts = entityStage::buildAccounts(rng, out.entities.people,
                                                     in.entities.population);

  out.entities.personas = entityStage::buildPersonas(rng, out.entities.people,
                                                     in.entities.personaMix);

  out.entities.pii =
      entityStage::buildPii(rng, out.entities.personas, identity);

  out.entities.merchants = entityStage::buildMerchants(
      rng, in.entities.population, in.entities.merchants);

  out.entities.landlords = entityStage::buildLandlords(
      rng, in.entities.population, in.entities.landlords);

  out.entities.creditCards = entityStage::issueCreditCards(
      out.entities.personas, out.entities.people, in.entities.seeds,
      in.entities.cardIssuance);

  out.entities.counterpartyPools = entityStage::buildCounterparties(
      rng, in.entities.population, in.entities.counterparties);

  synthesizeProducts(out, in.window, in.entities);

  out.infra = ::PhantomLedger::pipeline::stages::infra::build(rng, out.entities,
                                                              infraIn);

  auto transfersIn = in.transfersIn;
  if (transfersIn.window.days == 0) {
    transfersIn.window = in.window;
  }

  if (transfersIn.seed == 0) {
    transfersIn.seed = in.seed;
  }

  out.transfers = ::PhantomLedger::pipeline::stages::transfers::build(
      rng, out.entities, out.infra, transfersIn);

  return out;
}

} // namespace PhantomLedger::pipeline
