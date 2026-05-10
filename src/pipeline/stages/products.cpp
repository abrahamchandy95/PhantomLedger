#include "phantomledger/pipeline/stages/products.hpp"

namespace PhantomLedger::pipeline::stages::products {

namespace productSynth = ::PhantomLedger::entities::synth::products;

ObligationSynthesis &ObligationSynthesis::seed(std::uint64_t value) noexcept {
  seed_ = value;
  return *this;
}

ObligationSynthesis &
ObligationSynthesis::mortgage(MortgageTerms value) noexcept {
  mortgage_ = value;
  return *this;
}

ObligationSynthesis &
ObligationSynthesis::autoLoan(AutoLoanTerms value) noexcept {
  autoLoan_ = value;
  return *this;
}

ObligationSynthesis &
ObligationSynthesis::studentLoan(StudentLoanTerms value) noexcept {
  studentLoan_ = value;
  return *this;
}

ObligationSynthesis &ObligationSynthesis::tax(TaxTerms value) noexcept {
  tax_ = value;
  return *this;
}

ObligationSynthesis &
ObligationSynthesis::insurance(InsuranceTerms value) noexcept {
  insurance_ = value;
  return *this;
}

void ObligationSynthesis::synthesize(
    ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::Window window) const {
  const auto &assignment = entities.personas.assignment;
  const auto population = static_cast<::PhantomLedger::entity::PersonId>(
      assignment.byPerson.size());

  for (::PhantomLedger::entity::PersonId person = 1; person <= population;
       ++person) {
    auto local = productSynth::personRng(seed_, person);
    const auto persona = assignment.byPerson[person - 1];

    auto &loans = entities.portfolios.loans();
    auto &obligations = entities.portfolios.obligations();

    productSynth::MortgageEmitter mortgageEmitter{local, window, mortgage_};
    productSynth::AutoLoanEmitter autoLoanEmitter{local, window, autoLoan_};
    productSynth::StudentLoanEmitter studentLoanEmitter{local, window,
                                                        studentLoan_};
    productSynth::TaxEmitter taxEmitter{local, obligations, window, tax_};
    productSynth::InsuranceEmitter insuranceEmitter{
        local, entities.portfolios.insurance(), insurance_};

    const bool hasMortgage =
        mortgageEmitter.emit(person, persona, loans, obligations);
    const bool hasAutoLoan =
        autoLoanEmitter.emit(person, persona, loans, obligations);

    (void)studentLoanEmitter.emit(person, persona, loans, obligations);
    (void)taxEmitter.emit(person, persona);

    (void)insuranceEmitter.emit(
        person, persona,
        productSynth::LoanAnchors{
            .hasMortgage = hasMortgage,
            .hasAutoLoan = hasAutoLoan,
            .mortgageP = mortgage_.adoption.probability(persona),
            .autoLoanP = autoLoan_.adoption.probability(persona),
        });
  }

  entities.portfolios.obligations().sort();
}

} // namespace PhantomLedger::pipeline::stages::products
