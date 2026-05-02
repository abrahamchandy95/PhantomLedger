#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entities/synth/products/auto_loan.hpp"
#include "phantomledger/entities/synth/products/insurance.hpp"
#include "phantomledger/entities/synth/products/mortgage.hpp"
#include "phantomledger/entities/synth/products/student_loan.hpp"
#include "phantomledger/entities/synth/products/tax.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

inline constexpr std::uint64_t kDefaultProductsSeed = 0xB0A7F00DULL;

[[nodiscard]] ::PhantomLedger::entity::product::PortfolioRegistry
build(::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
      const ::PhantomLedger::entities::synth::personas::Pack &personas,
      const ::PhantomLedger::entity::card::Registry &creditCards,
      std::uint64_t baseSeed = kDefaultProductsSeed,
      const MortgageTerms &mortgage = MortgageTerms{},
      const AutoLoanTerms &autoLoan = AutoLoanTerms{},
      const StudentLoanTerms &studentLoan = StudentLoanTerms{},
      const TaxTerms &tax = TaxTerms{},
      const InsuranceTerms &insurance = InsuranceTerms{});

} // namespace PhantomLedger::entities::synth::products
