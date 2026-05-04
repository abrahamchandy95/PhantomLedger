#include "phantomledger/pipeline/simulate.hpp"

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = ::PhantomLedger::pipeline::stages::entities;
namespace infraStage = ::PhantomLedger::pipeline::stages::infra;
namespace transferStage = ::PhantomLedger::pipeline::stages::transfers;
namespace productSynth = ::PhantomLedger::entities::synth::products;

[[nodiscard]] ::PhantomLedger::time::Window
activeInfraWindow(const SimulationScenario &scenario) noexcept {
  if (scenario.infraWindow.days == 0) {
    return scenario.window;
  }

  return scenario.infraWindow;
}

void synthesizeProducts(SimulationResult &out,
                        ::PhantomLedger::time::Window window,
                        const entityStage::ProductSynthesis &products) {
  const auto &assignment = out.entities.personas.assignment;
  const auto population = static_cast<::PhantomLedger::entity::PersonId>(
      assignment.byPerson.size());

  for (::PhantomLedger::entity::PersonId person = 1; person <= population;
       ++person) {
    auto local = productSynth::personRng(products.seeds.products, person);
    const auto persona = assignment.byPerson[person - 1];

    productSynth::MortgageEmitter mortgage{local, out.entities.portfolios,
                                           window, products.mortgage};
    productSynth::AutoLoanEmitter autoLoan{local, out.entities.portfolios,
                                           window, products.autoLoan};
    productSynth::StudentLoanEmitter studentLoan{local, out.entities.portfolios,
                                                 window, products.studentLoan};
    productSynth::TaxEmitter tax{local, out.entities.portfolios, window,
                                 products.tax};
    productSynth::InsuranceEmitter insurance{local, out.entities.portfolios,
                                             products.insurance};

    const bool hasMortgage = mortgage.emit(person, persona);
    const bool hasAutoLoan = autoLoan.emit(person, persona);

    (void)studentLoan.emit(person, persona);
    (void)tax.emit(person, persona);

    (void)insurance.emit(
        person, persona,
        productSynth::LoanAnchors{
            .hasMortgage = hasMortgage,
            .hasAutoLoan = hasAutoLoan,
            .mortgageP = products.mortgage.adoption.probability(persona),
            .autoLoanP = products.autoLoan.adoption.probability(persona),
        });
  }

  out.entities.portfolios.obligations().sort();
}

} // namespace

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          const SimulationScenario &scenario) {
  SimulationResult out;

  entityStage::validate(scenario.entities.people.population);

  const auto identity = entityStage::withDefaultStart(
      scenario.entities.people.identity, scenario.window.start);

  out.entities.people = entityStage::buildPeople(
      rng, scenario.entities.people.population, scenario.entities.people.fraud);

  out.entities.accounts = entityStage::buildAccounts(
      rng, out.entities.people, scenario.entities.people.population);

  out.entities.personas = entityStage::buildPersonas(
      rng, out.entities.people, scenario.entities.people.personaMix);

  out.entities.pii =
      entityStage::buildPii(rng, out.entities.personas, identity);

  out.entities.merchants =
      entityStage::buildMerchants(rng, scenario.entities.people.population,
                                  scenario.entities.counterparties.merchants);

  out.entities.landlords =
      entityStage::buildLandlords(rng, scenario.entities.people.population,
                                  scenario.entities.counterparties.landlords);

  out.entities.creditCards =
      entityStage::issueCreditCards(out.entities.personas, out.entities.people,
                                    scenario.entities.products.seeds,
                                    scenario.entities.products.cardIssuance);

  out.entities.counterparties = entityStage::buildCounterparties(
      rng, scenario.entities.people.population,
      scenario.entities.counterparties.counterparties);

  // Register every external endpoint (system bank accounts, merchants,
  // landlords, employers, clients, platforms, processors, businesses,
  // brokerages) into entities.accounts so that:
  //   - validateTransactionAccounts() in the transfer stage finds every
  //     transaction endpoint in the lookup,
  //   - hubIndicesFromKeys() resolves counterparty hub keys against the
  //     lookup so balance-book bootstrap can grant infinite cash to hubs,
  //   - the standard exporter's external_accounts.csv pass and the AML
  //     SharedContext counterparty set both pick these up via the
  //     Flag::external bit.
  // Must run after every entity-build step above and before transfer build.
  entityStage::finalizeAccountRegistry(out.entities);

  synthesizeProducts(out, scenario.window, scenario.entities.products);

  out.infra = infraStage::build(rng, out.entities, activeInfraWindow(scenario),
                                scenario.ringBehavior, scenario.deviceBehavior,
                                scenario.ipBehavior, scenario.routingBehavior,
                                scenario.sharedInfra);

  auto transferScope = scenario.transfers.run;
  if (transferScope.window.days == 0) {
    transferScope.window = scenario.window;
  }

  if (transferScope.seed == 0) {
    transferScope.seed = scenario.seed;
  }

  out.transfers = transferStage::build(
      rng, out.entities, out.infra, transferScope,
      scenario.transfers.recurringIncome, scenario.transfers.balanceBook,
      scenario.transfers.creditCards, scenario.transfers.family,
      scenario.transfers.government, scenario.transfers.insurance,
      scenario.transfers.replay, scenario.transfers.fraud,
      scenario.transfers.population);

  return out;
}

} // namespace PhantomLedger::pipeline
