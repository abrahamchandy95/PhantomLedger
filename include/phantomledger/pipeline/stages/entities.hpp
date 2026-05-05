#pragma once

#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/counterparties/make.hpp"
#include "phantomledger/entities/synth/landlords/make.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/merchants/make.hpp"
#include "phantomledger/entities/synth/merchants/pack.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/entities/synth/personas/kinds.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {
struct Entities;
} // namespace PhantomLedger::pipeline

namespace PhantomLedger::pipeline::stages::entities {

struct PopulationSizing {
  std::int32_t count = 10'000;
  std::int32_t maxAccountsPerPerson = 3;
};

struct IdentitySource {
  const ::PhantomLedger::entities::synth::pii::PoolSet &pools;
  ::PhantomLedger::time::TimePoint simStart{};
  ::PhantomLedger::entities::synth::pii::LocaleMix localeMix =
      ::PhantomLedger::entities::synth::pii::LocaleMix::usBankDefault();
};

struct PeopleSynthesis {
  IdentitySource identity;
  PopulationSizing population{};
  ::PhantomLedger::entities::synth::people::Fraud fraud{};
  ::PhantomLedger::entities::synth::personas::Mix personaMix{};
};

struct CounterpartySynthesis {
  ::PhantomLedger::entities::synth::merchants::GenerationPlan merchants{};
  ::PhantomLedger::entities::synth::landlords::GenerationPlan landlords{};
  ::PhantomLedger::entities::synth::counterparties::CounterpartyTargets
      counterparties{};
};

struct CardSynthesis {
  std::uint64_t seed = 0xC0DECAFEULL;
  ::PhantomLedger::entities::synth::cards::IssuanceRules issuance{};
};

struct EntitySynthesis {
  PeopleSynthesis people;
  CounterpartySynthesis counterparties{};
  CardSynthesis cards{};
};

void validate(PopulationSizing population);

[[nodiscard]] IdentitySource
withDefaultStart(IdentitySource identity,
                 ::PhantomLedger::time::TimePoint fallbackStart);

[[nodiscard]] ::PhantomLedger::entities::synth::people::Pack
buildPeople(::PhantomLedger::random::Rng &rng, PopulationSizing population,
            const ::PhantomLedger::entities::synth::people::Fraud &fraud = {});

[[nodiscard]] ::PhantomLedger::entities::synth::accounts::Pack
buildAccounts(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::entities::synth::people::Pack &people,
              PopulationSizing population);

[[nodiscard]] ::PhantomLedger::entities::synth::personas::Pack
buildPersonas(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::entities::synth::people::Pack &people,
              const ::PhantomLedger::entities::synth::personas::Mix &mix = {});

[[nodiscard]] ::PhantomLedger::entity::pii::Roster
buildPii(::PhantomLedger::random::Rng &rng,
         const ::PhantomLedger::entities::synth::personas::Pack &personas,
         IdentitySource identity);

[[nodiscard]] ::PhantomLedger::entities::synth::merchants::Pack
buildMerchants(::PhantomLedger::random::Rng &rng, PopulationSizing population,
               const ::PhantomLedger::entities::synth::merchants::GenerationPlan
                   &plan = {});

[[nodiscard]] ::PhantomLedger::entities::synth::landlords::Pack
buildLandlords(::PhantomLedger::random::Rng &rng, PopulationSizing population,
               const ::PhantomLedger::entities::synth::landlords::GenerationPlan
                   &plan = {});

[[nodiscard]] ::PhantomLedger::entity::card::Registry issueCreditCards(
    const ::PhantomLedger::entities::synth::personas::Pack &personas,
    const ::PhantomLedger::entities::synth::people::Pack &people,
    const CardSynthesis &cards = {});

[[nodiscard]] ::PhantomLedger::entity::counterparty::Directory
buildCounterparties(
    ::PhantomLedger::random::Rng &rng, PopulationSizing population,
    const ::PhantomLedger::entities::synth::counterparties::CounterpartyTargets
        &targets = {});

void finalizeAccountRegistry(::PhantomLedger::pipeline::Entities &entities);

} // namespace PhantomLedger::pipeline::stages::entities
