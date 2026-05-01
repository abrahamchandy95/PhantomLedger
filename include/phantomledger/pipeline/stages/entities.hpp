#pragma once

#include "phantomledger/entities/synth/cards/policy.hpp"
#include "phantomledger/entities/synth/counterparties/config.hpp"
#include "phantomledger/entities/synth/landlords/config.hpp"
#include "phantomledger/entities/synth/merchants/config.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/personas/kinds.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/entities/synth/products/configs.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::entities {

struct Inputs {
  std::int32_t population = 10'000;

  std::int32_t maxAccountsPerPerson = 3;

  time::TimePoint simStart{};

  ::PhantomLedger::time::Window window{};

  ::PhantomLedger::entities::synth::people::Fraud fraud{};
  ::PhantomLedger::entities::synth::personas::Mix personaMix{};
  ::PhantomLedger::entities::synth::merchants::Config merchantsCfg{};
  ::PhantomLedger::entities::synth::landlords::Config landlordsCfg{};
  ::PhantomLedger::entities::synth::counterparties::Config counterpartiesCfg{};
  ::PhantomLedger::entities::synth::cards::Policy cardsPolicy =
      ::PhantomLedger::entities::synth::cards::kDefaultPolicy;

  ::PhantomLedger::entities::synth::products::MortgageConfig mortgageCfg =
      ::PhantomLedger::entities::synth::products::kDefaultMortgage;
  ::PhantomLedger::entities::synth::products::AutoLoanConfig autoLoanCfg =
      ::PhantomLedger::entities::synth::products::kDefaultAutoLoan;
  ::PhantomLedger::entities::synth::products::StudentLoanConfig studentLoanCfg =
      ::PhantomLedger::entities::synth::products::kDefaultStudentLoan;
  ::PhantomLedger::entities::synth::products::TaxConfig taxCfg =
      ::PhantomLedger::entities::synth::products::kDefaultTax;
  ::PhantomLedger::entities::synth::products::InsuranceConfig insuranceCfg =
      ::PhantomLedger::entities::synth::products::kDefaultInsurance;

  ::PhantomLedger::entities::synth::pii::LocaleMix localeMix =
      ::PhantomLedger::entities::synth::pii::LocaleMix::usBankDefault();
  const ::PhantomLedger::entities::synth::pii::PoolSet *piiPools = nullptr;

  std::uint64_t cardsBaseSeed = 0xC0DECAFEULL;

  std::uint64_t productsBaseSeed = 0xB0A7F00DULL;
};

[[nodiscard]] ::PhantomLedger::pipeline::Entities
build(::PhantomLedger::random::Rng &rng, const Inputs &in);

} // namespace PhantomLedger::pipeline::stages::entities
