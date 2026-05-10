#include "phantomledger/entities/synth/products/student_loan.hpp"

#include "phantomledger/entities/synth/products/amount_sampling.hpp"
#include "phantomledger/entities/synth/products/dates.hpp"
#include "phantomledger/entities/synth/products/installment_emission.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"

#include <algorithm>
#include <utility>

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

StudentLoanEmitter::StudentLoanEmitter(::PhantomLedger::random::Rng &rng,
                                       ::PhantomLedger::time::Window window,
                                       StudentLoanTerms terms)
    : rng_{&rng}, window_{window}, terms_{std::move(terms)} {}

[[nodiscard]] bool StudentLoanEmitter::emit(
    ::PhantomLedger::entity::PersonId person, personaTax::Type persona,
    ::PhantomLedger::entity::product::LoanTermsLedger &loans,
    ::PhantomLedger::entity::product::ObligationStream &obligations) {
  if (rng_->nextDouble() >= terms_.adoption.probability(persona)) {
    return false;
  }

  const double payment = samplePaymentAmount(
      *rng_, terms_.payment.median, terms_.payment.sigma, terms_.payment.floor);

  const std::int32_t termMonths =
      sampleStudentTermMonths(*rng_, terms_.planMix, terms_.term);

  const std::int32_t repaymentAgeMonths = sampleStudentRepaymentAgeMonths(
      *rng_, persona, terms_.term, terms_.deferment);

  const auto repaymentStart =
      ::PhantomLedger::time::addMonths(window_.start, -repaymentAgeMonths);

  addInstallmentProduct(loans, obligations, window_,
                        InstallmentIssue{
                            .person = person,
                            .productType = product::ProductType::studentLoan,
                            .counterparty = institutional::studentServicer(),
                            .start = repaymentStart,
                            .termMonths = termMonths,
                            .paymentDay = samplePaymentDay(*rng_),
                            .monthlyPayment = payment,
                            .terms = installmentTerms(terms_.delinquency),
                        });

  return true;
}

} // namespace PhantomLedger::entities::synth::products
