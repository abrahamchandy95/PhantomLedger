#pragma once

#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/products/auto_loan.hpp"
#include "phantomledger/synth/products/insurance.hpp"
#include "phantomledger/synth/products/mortgage.hpp"
#include "phantomledger/synth/products/random.hpp"
#include "phantomledger/synth/products/student_loan.hpp"
#include "phantomledger/synth/products/tax.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::products {

class ObligationSynthesis {
public:
  using MortgageTerms = ::PhantomLedger::synth::products::MortgageTerms;
  using AutoLoanTerms = ::PhantomLedger::synth::products::AutoLoanTerms;
  using StudentLoanTerms = ::PhantomLedger::synth::products::StudentLoanTerms;
  using TaxTerms = ::PhantomLedger::synth::products::TaxTerms;
  using InsuranceTerms = ::PhantomLedger::synth::products::InsuranceTerms;

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
  std::uint64_t seed_ = ::PhantomLedger::synth::products::kDefaultProductsSeed;

  MortgageTerms mortgage_{};
  AutoLoanTerms autoLoan_{};
  StudentLoanTerms studentLoan_{};
  TaxTerms tax_{};
  InsuranceTerms insurance_{};
};

} // namespace PhantomLedger::pipeline::stages::products
