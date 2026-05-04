#include "phantomledger/pipeline/stages/products.hpp"

#include "phantomledger/entities/synth/products/auto_loan.hpp"
#include "phantomledger/entities/synth/products/insurance.hpp"
#include "phantomledger/entities/synth/products/mortgage.hpp"
#include "phantomledger/entities/synth/products/random.hpp"
#include "phantomledger/entities/synth/products/student_loan.hpp"
#include "phantomledger/entities/synth/products/tax.hpp"

namespace PhantomLedger::pipeline::stages::products {

namespace productSynth = ::PhantomLedger::entities::synth::products;

void synthesize(
    ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::Window window,
    const ::PhantomLedger::pipeline::stages::entities::ProductSynthesis
        &products) {
  const auto &assignment = entities.personas.assignment;
  const auto population = static_cast<::PhantomLedger::entity::PersonId>(
      assignment.byPerson.size());

  for (::PhantomLedger::entity::PersonId person = 1; person <= population;
       ++person) {
    auto local = productSynth::personRng(products.seeds.products, person);
    const auto persona = assignment.byPerson[person - 1];

    productSynth::MortgageEmitter mortgage{local, entities.portfolios, window,
                                           products.mortgage};
    productSynth::AutoLoanEmitter autoLoan{local, entities.portfolios, window,
                                           products.autoLoan};
    productSynth::StudentLoanEmitter studentLoan{local, entities.portfolios,
                                                 window, products.studentLoan};
    productSynth::TaxEmitter tax{local, entities.portfolios, window,
                                 products.tax};
    productSynth::InsuranceEmitter insurance{local, entities.portfolios,
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

  entities.portfolios.obligations().sort();
}

} // namespace PhantomLedger::pipeline::stages::products
