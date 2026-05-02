#pragma once

#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/counterparties/config.hpp"
#include "phantomledger/entities/synth/landlords/config.hpp"
#include "phantomledger/entities/synth/merchants/config.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/personas/kinds.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/entities/synth/products/build.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::entities {

struct PopulationPlan {
  std::int32_t count = 10'000;
  std::int32_t maxAccountsPerPerson = 3;
};

struct IdentitySource {
  const ::PhantomLedger::entities::synth::pii::PoolSet &pools;
  ::PhantomLedger::time::TimePoint simStart{};
  ::PhantomLedger::entities::synth::pii::LocaleMix localeMix =
      ::PhantomLedger::entities::synth::pii::LocaleMix::usBankDefault();
};

struct Seeds {
  std::uint64_t cardIssuance = 0xC0DECAFEULL;
  std::uint64_t products =
      ::PhantomLedger::entities::synth::products::kDefaultProductsSeed;
};

[[nodiscard]] ::PhantomLedger::pipeline::Entities build(
    ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
    IdentitySource identity, PopulationPlan population = {},
    ::PhantomLedger::entities::synth::people::Fraud fraud = {},
    ::PhantomLedger::entities::synth::personas::Mix personaMix = {},
    ::PhantomLedger::entities::synth::merchants::Config merchants = {},
    ::PhantomLedger::entities::synth::landlords::Config landlords = {},
    ::PhantomLedger::entities::synth::counterparties::Config counterparties =
        {},
    ::PhantomLedger::entities::synth::cards::IssuanceRules cardIssuance = {},
    Seeds seeds = {},
    ::PhantomLedger::entities::synth::products::MortgageTerms mortgage = {},
    ::PhantomLedger::entities::synth::products::AutoLoanTerms autoLoan = {},
    ::PhantomLedger::entities::synth::products::StudentLoanTerms studentLoan =
        {},
    ::PhantomLedger::entities::synth::products::TaxTerms tax = {},
    ::PhantomLedger::entities::synth::products::InsuranceTerms insurance = {});

} // namespace PhantomLedger::pipeline::stages::entities
