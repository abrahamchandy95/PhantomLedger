#pragma once

#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/products/random.hpp"
#include "phantomledger/synth/products/terms/auto_loan.hpp"
#include "phantomledger/synth/products/terms/insurance.hpp"
#include "phantomledger/synth/products/terms/mortgage.hpp"
#include "phantomledger/synth/products/terms/student_loan.hpp"
#include "phantomledger/synth/products/terms/tax.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

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

  // RAM R2.2.1 (docs/ram_derive_dont_store.md): replay the identical
  // per-person emission — same content-keyed draws, same append order —
  // keeping only events with timestamps in [start, endExcl). Terms land
  // in scratch ledgers and are discarded. Under the stream's pinned
  // total order (timestamp, then append order; stable sort) every
  // timestamp's tie group is independent, so the result is
  // byte-identical to the corresponding between() slice of the
  // synthesize()-materialized stream. `window` must be the FULL run
  // window (emission draws depend on it), never the chunk.
  [[nodiscard]] ::PhantomLedger::entity::product::ObligationStream
  generateWindow(const People &people, time::Window window,
                 time::TimePoint start, time::TimePoint endExcl) const;

private:
  // The one emission sequence shared by synthesize() and
  // generateWindow(), so the materialized stream and the windowed
  // replay cannot drift.
  void emitPerson(::PhantomLedger::entity::PersonId person,
                  ::PhantomLedger::personas::Type persona, time::Window window,
                  ::PhantomLedger::entity::product::LoanTermsLedger &loans,
                  ::PhantomLedger::entity::product::InsuranceLedger &insurance,
                  ::PhantomLedger::entity::product::ObligationStream
                      &obligations) const;

  std::uint64_t seed_ = ::PhantomLedger::synth::products::kDefaultProductsSeed;

  MortgageTerms mortgage_{};
  AutoLoanTerms autoLoan_{};
  StudentLoanTerms studentLoan_{};
  TaxTerms tax_{};
  InsuranceTerms insurance_{};
};

} // namespace PhantomLedger::pipeline::stages::products
