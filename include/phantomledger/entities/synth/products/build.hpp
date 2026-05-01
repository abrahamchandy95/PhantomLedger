#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entities/synth/products/configs.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

struct Inputs {
  ::PhantomLedger::time::Window window{};

  const ::PhantomLedger::entities::synth::personas::Pack *personas = nullptr;

  const ::PhantomLedger::entity::card::Registry *creditCards = nullptr;

  /// Per-product configs.
  MortgageConfig mortgageCfg = kDefaultMortgage;
  AutoLoanConfig autoLoanCfg = kDefaultAutoLoan;
  StudentLoanConfig studentLoanCfg = kDefaultStudentLoan;
  TaxConfig taxCfg = kDefaultTax;
  InsuranceConfig insuranceCfg = kDefaultInsurance;

  std::uint64_t baseSeed = 0xB0A7F00DULL;
};

[[nodiscard]] ::PhantomLedger::entity::product::PortfolioRegistry
build(::PhantomLedger::random::Rng &rng, const Inputs &in);

} // namespace PhantomLedger::entities::synth::products
