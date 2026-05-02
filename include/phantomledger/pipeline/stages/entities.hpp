#pragma once

#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/counterparties/config.hpp"
#include "phantomledger/entities/synth/landlords/config.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/merchants/config.hpp"
#include "phantomledger/entities/synth/merchants/pack.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/entities/synth/personas/kinds.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/entities/synth/products/random.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

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

void validate(PopulationPlan population);

[[nodiscard]] IdentitySource
withDefaultStart(IdentitySource identity,
                 ::PhantomLedger::time::TimePoint fallbackStart);

[[nodiscard]] ::PhantomLedger::entities::synth::people::Pack
buildPeople(::PhantomLedger::random::Rng &rng, PopulationPlan population,
            const ::PhantomLedger::entities::synth::people::Fraud &fraud = {});

[[nodiscard]] ::PhantomLedger::entities::synth::accounts::Pack
buildAccounts(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::entities::synth::people::Pack &people,
              PopulationPlan population);

[[nodiscard]] ::PhantomLedger::entities::synth::personas::Pack
buildPersonas(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::entities::synth::people::Pack &people,
              const ::PhantomLedger::entities::synth::personas::Mix &mix = {});

[[nodiscard]] ::PhantomLedger::entity::pii::Roster
buildPii(::PhantomLedger::random::Rng &rng,
         const ::PhantomLedger::entities::synth::personas::Pack &personas,
         IdentitySource identity);

[[nodiscard]] ::PhantomLedger::entities::synth::merchants::Pack buildMerchants(
    ::PhantomLedger::random::Rng &rng, PopulationPlan population,
    const ::PhantomLedger::entities::synth::merchants::Config &config = {});

[[nodiscard]] ::PhantomLedger::entities::synth::landlords::Pack buildLandlords(
    ::PhantomLedger::random::Rng &rng, PopulationPlan population,
    const ::PhantomLedger::entities::synth::landlords::Config &config = {});

[[nodiscard]] ::PhantomLedger::entity::card::Registry issueCreditCards(
    const ::PhantomLedger::entities::synth::personas::Pack &personas,
    const ::PhantomLedger::entities::synth::people::Pack &people, Seeds seeds,
    const ::PhantomLedger::entities::synth::cards::IssuanceRules &rules = {});

[[nodiscard]] ::PhantomLedger::entity::counterparty::Pool buildCounterparties(
    ::PhantomLedger::random::Rng &rng, PopulationPlan population,
    const ::PhantomLedger::entities::synth::counterparties::Config &config =
        {});

} // namespace PhantomLedger::pipeline::stages::entities
