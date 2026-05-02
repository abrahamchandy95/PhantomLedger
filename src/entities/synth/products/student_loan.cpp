#include "phantomledger/entities/synth/products/student_loan.hpp"

#include "phantomledger/entities/synth/products/amount_sampling.hpp"
#include "phantomledger/entities/synth/products/dates.hpp"
#include "phantomledger/entities/synth/products/installment_emission.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"

#include <algorithm>

namespace PhantomLedger::entities::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;

[[nodiscard]] std::int32_t
sampleStudentTermMonths(::PhantomLedger::random::Rng &rng,
                        const StudentLoanPlanMix &planMix,
                        const StudentLoanTerm &term) {
  const double total = planMix.standardP + planMix.extendedP + planMix.idrLikeP;

  if (total <= 0.0) {
    return term.standardMonths;
  }

  const double u = rng.nextDouble() * total;

  if (u < planMix.standardP) {
    return term.standardMonths;
  }

  if (u < planMix.standardP + planMix.extendedP) {
    return term.extendedMonths;
  }

  return rng.coin(term.idr20YearP) ? term.idr20YearMonths
                                   : term.idr25YearMonths;
}

[[nodiscard]] std::int32_t sampleStudentRepaymentAgeMonths(
    ::PhantomLedger::random::Rng &rng, personaTax::Type persona,
    const StudentLoanTerm &term, const StudentLoanDeferment &deferment) {
  if (persona == personaTax::Type::student) {
    if (rng.coin(deferment.studentP)) {
      const auto graceMonths =
          std::max<std::int32_t>(1, deferment.gracePeriodMonths);

      return -static_cast<std::int32_t>(
          rng.uniformInt(1, static_cast<std::int64_t>(graceMonths) + 1));
    }

    return static_cast<std::int32_t>(rng.uniformInt(1, 13));
  }

  return static_cast<std::int32_t>(
      rng.uniformInt(1, static_cast<std::int64_t>(term.standardMonths) + 1));
}

} // namespace

[[nodiscard]] bool
emitStudentLoan(::PhantomLedger::random::Rng &rng,
                ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
                ::PhantomLedger::entity::PersonId person,
                personaTax::Type persona, ::PhantomLedger::time::Window window,
                const StudentLoanTerms &terms) {
  if (rng.nextDouble() >= terms.adoption.probability(persona)) {
    return false;
  }

  const double payment = samplePaymentAmount(
      rng, terms.payment.median, terms.payment.sigma, terms.payment.floor);

  const std::int32_t termMonths =
      sampleStudentTermMonths(rng, terms.planMix, terms.term);

  const std::int32_t repaymentAgeMonths = sampleStudentRepaymentAgeMonths(
      rng, persona, terms.term, terms.deferment);

  const auto repaymentStart =
      ::PhantomLedger::time::addMonths(window.start, -repaymentAgeMonths);

  addInstallmentProduct(portfolios, person, product::ProductType::studentLoan,
                        institutional::studentServicer(), repaymentStart,
                        termMonths, samplePaymentDay(rng), payment, window,
                        delinquencyKnobs(terms.delinquency));

  return true;
}

} // namespace PhantomLedger::entities::synth::products
