#include "phantomledger/pipeline/stages/products.hpp"

#include "phantomledger/transfers/legit/ledger/burdens.hpp"

namespace PhantomLedger::pipeline::stages::products {

namespace productSynth = ::PhantomLedger::synth::products;

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

void ObligationSynthesis::emitPerson(
    ::PhantomLedger::entity::PersonId person,
    ::PhantomLedger::personas::Type persona,
    ::PhantomLedger::time::Window window,
    ::PhantomLedger::entity::product::LoanTermsLedger &loans,
    ::PhantomLedger::entity::product::InsuranceLedger &insurance,
    ::PhantomLedger::entity::product::ObligationStream &obligations) const {
  // Content-keyed per person: this sequence is replayable in isolation.
  // Emitter construction and emit order are draw-order-defining — any
  // change here is a model change.
  auto local = productSynth::personRng(seed_, person);

  productSynth::MortgageEmitter mortgageEmitter{local, window, mortgage_};
  productSynth::AutoLoanEmitter autoLoanEmitter{local, window, autoLoan_};
  productSynth::StudentLoanEmitter studentLoanEmitter{local, window,
                                                      studentLoan_};
  productSynth::TaxEmitter taxEmitter{local, obligations, window, tax_};
  productSynth::InsuranceEmitter insuranceEmitter{local, insurance,
                                                  insurance_};

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

void ObligationSynthesis::synthesize(
    const ::PhantomLedger::pipeline::People &people,
    ::PhantomLedger::pipeline::Holdings &holdings,
    ::PhantomLedger::time::Window window) const {

  const auto &assignment = people.personas.assignment;
  const auto population = static_cast<::PhantomLedger::entity::PersonId>(
      assignment.byPerson.size());

  // RAM R2.2.1c: retain only the burden slice. buildMonthlyBurdens is
  // the stream's sole resident reader, and both of its call sites (the
  // opening-book burden buffer, the spending prep) query exactly
  // [window.start, window.start + kBurdenWindowMonths x 30 days) — the
  // same arithmetic as its addMonths helper. The full window is derived
  // on demand by generateWindow(). Emission (and therefore every draw)
  // is unchanged; out-of-slice events are dropped at append.
  auto &obligations = holdings.portfolios.obligations();
  obligations.restrictTo(
      window.start,
      time::addDays(window.start,
                    30 * ::PhantomLedger::transfers::legit::ledger::
                             kBurdenWindowMonths));

  for (::PhantomLedger::entity::PersonId person = 1; person <= population;
       ++person) {
    emitPerson(person, assignment.byPerson[person - 1], window,
               holdings.portfolios.loans(), holdings.portfolios.insurance(),
               obligations);
  }

  obligations.sort();
}

::PhantomLedger::entity::product::ObligationStream
ObligationSynthesis::generateWindow(
    const ::PhantomLedger::pipeline::People &people,
    ::PhantomLedger::time::Window window, ::PhantomLedger::time::TimePoint start,
    ::PhantomLedger::time::TimePoint endExcl) const {

  const auto &assignment = people.personas.assignment;
  const auto population = static_cast<::PhantomLedger::entity::PersonId>(
      assignment.byPerson.size());

  // The draws interleave terms and events per person, so the replay must
  // run the full emitters; the terms land in scratch ledgers and only
  // the in-range events survive (chunk-sized, enforced at append).
  ::PhantomLedger::entity::product::LoanTermsLedger scratchLoans;
  ::PhantomLedger::entity::product::InsuranceLedger scratchInsurance;

  ::PhantomLedger::entity::product::ObligationStream out;
  out.restrictTo(start, endExcl);

  for (::PhantomLedger::entity::PersonId person = 1; person <= population;
       ++person) {
    emitPerson(person, assignment.byPerson[person - 1], window, scratchLoans,
               scratchInsurance, out);
  }

  // In-range events arrive in global append order, so the stable sort
  // reproduces the materialized stream's between(start, endExcl) slice
  // exactly.
  out.sort();
  return out;
}

} // namespace PhantomLedger::pipeline::stages::products
