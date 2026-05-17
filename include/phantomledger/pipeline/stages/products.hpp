#pragma once

#include "phantomledger/entities/synth/products/auto_loan.hpp"
#include "phantomledger/entities/synth/products/insurance.hpp"
#include "phantomledger/entities/synth/products/mortgage.hpp"
#include "phantomledger/entities/synth/products/random.hpp"
#include "phantomledger/entities/synth/products/student_loan.hpp"
#include "phantomledger/entities/synth/products/tax.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::products {

class ObligationSynthesis {
public:
  using MortgageTerms =
      ::PhantomLedger::entities::synth::products::MortgageTerms;
  using AutoLoanTerms =
      ::PhantomLedger::entities::synth::products::AutoLoanTerms;
  using StudentLoanTerms =
      ::PhantomLedger::entities::synth::products::StudentLoanTerms;
  using TaxTerms = ::PhantomLedger::entities::synth::products::TaxTerms;
  using InsuranceTerms =
      ::PhantomLedger::entities::synth::products::InsuranceTerms;

  ObligationSynthesis() = default;

  ObligationSynthesis &seed(std::uint64_t value) noexcept;
  ObligationSynthesis &mortgage(MortgageTerms value) noexcept;
  ObligationSynthesis &autoLoan(AutoLoanTerms value) noexcept;
  ObligationSynthesis &studentLoan(StudentLoanTerms value) noexcept;
  ObligationSynthesis &tax(TaxTerms value) noexcept;
  ObligationSynthesis &insurance(InsuranceTerms value) noexcept;

  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
  [[nodiscard]] const MortgageTerms &mortgage() const noexcept {
    return mortgage_;
  }
  [[nodiscard]] const AutoLoanTerms &autoLoan() const noexcept {
    return autoLoan_;
  }
  [[nodiscard]] const StudentLoanTerms &studentLoan() const noexcept {
    return studentLoan_;
  }
  [[nodiscard]] const TaxTerms &tax() const noexcept { return tax_; }
  [[nodiscard]] const InsuranceTerms &insurance() const noexcept {
    return insurance_;
  }

  void synthesize(const People &people, Holdings &holdings,
                  time::Window window) const;

private:
  std::uint64_t seed_ =
      ::PhantomLedger::entities::synth::products::kDefaultProductsSeed;

  MortgageTerms mortgage_{};
  AutoLoanTerms autoLoan_{};
  StudentLoanTerms studentLoan_{};
  TaxTerms tax_{};
  InsuranceTerms insurance_{};
};

} // namespace PhantomLedger::pipeline::stages::products
