#pragma once

#include "phantomledger/entities/synth/products/auto_loan.hpp"
#include "phantomledger/entities/synth/products/insurance.hpp"
#include "phantomledger/entities/synth/products/mortgage.hpp"
#include "phantomledger/entities/synth/products/random.hpp"
#include "phantomledger/entities/synth/products/student_loan.hpp"
#include "phantomledger/entities/synth/products/tax.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::products {

struct PersonProductRng {
  std::uint64_t seed =
      ::PhantomLedger::entities::synth::products::kDefaultProductsSeed;
};

struct InstallmentLending {
  ::PhantomLedger::entities::synth::products::MortgageTerms mortgage{};
  ::PhantomLedger::entities::synth::products::AutoLoanTerms autoLoan{};
  ::PhantomLedger::entities::synth::products::StudentLoanTerms studentLoan{};
};

struct TaxWithholding {
  ::PhantomLedger::entities::synth::products::TaxTerms tax{};
};

struct InsuranceCoverage {
  ::PhantomLedger::entities::synth::products::InsuranceTerms insurance{};
};

struct ObligationPrograms {
  PersonProductRng rng{};
  InstallmentLending lending{};
  TaxWithholding taxes{};
  InsuranceCoverage coverage{};
};

void synthesize(::PhantomLedger::pipeline::Entities &entities,
                ::PhantomLedger::time::Window window,
                const ObligationPrograms &programs);

} // namespace PhantomLedger::pipeline::stages::products
