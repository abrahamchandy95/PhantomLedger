#pragma once

#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"
#include "phantomledger/entities/synth/accounts/sizing.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/counterparties/make.hpp"
#include "phantomledger/entities/synth/landlords/make.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/merchants/make.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/entities/synth/personas/kinds.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {
struct Entities;
} // namespace PhantomLedger::pipeline

namespace PhantomLedger::pipeline::stages::entities {

namespace pl = ::PhantomLedger;
namespace synth = pl::entities::synth;
namespace entity = pl::entity;

struct IdentitySource {
  const synth::pii::PoolSet *pools = nullptr;
  pl::time::TimePoint simStart;
  synth::pii::LocaleMix localeMix;
};

struct EntitySynthesis {
  std::int32_t population;
  IdentitySource identity;

  synth::people::Fraud fraud{};
  synth::personas::Mix personaMix{};
  synth::accounts::Sizing accountsSizing{};
  synth::merchants::GenerationPlan merchants{};
  synth::landlords::GenerationPlan landlords{};
  synth::counterparties::CounterpartyTargets counterpartyTargets{};
  synth::cards::IssuanceRules cards{};
  void validate(pl::primitives::validate::Report &r) const {
    r.check([&] {
      pl::primitives::validate::nonNegative("population", population);
    });

    accountsSizing.validate(r);
    merchants.validate(r);
    landlords.validate(r);
    counterpartyTargets.validate(r);
  }
};

[[nodiscard]] IdentitySource defaultStart(IdentitySource identity,
                                          pl::time::TimePoint fallbackStart);

[[nodiscard]] synth::people::Pack
buildPeople(pl::random::Rng &rng, std::int32_t population,
            const synth::people::Fraud &fraud = {});

[[nodiscard]] synth::accounts::Pack
buildAccounts(pl::random::Rng &rng, const synth::people::Pack &people,
              std::int32_t population,
              const synth::accounts::Sizing &sizing = {});

[[nodiscard]] synth::personas::Pack
buildPersonas(pl::random::Rng &rng, const synth::people::Pack &people,
              const synth::personas::Mix &mix = {});

[[nodiscard]] entity::pii::Roster
buildPii(pl::random::Rng &rng, const synth::personas::Pack &personas,
         IdentitySource identity);

[[nodiscard]] entity::merchant::Catalog
buildMerchants(pl::random::Rng &rng, std::int32_t population,
               const synth::merchants::GenerationPlan &plan = {});

[[nodiscard]] synth::landlords::Pack
buildLandlords(pl::random::Rng &rng, std::int32_t population,
               const synth::landlords::GenerationPlan &plan = {});

[[nodiscard]] entity::card::Registry
issueCreditCards(const synth::personas::Pack &personas,
                 const synth::people::Pack &people, std::uint64_t topLevelSeed,
                 const synth::cards::IssuanceRules &issuance = {});

[[nodiscard]] entity::counterparty::Directory buildCounterparties(
    pl::random::Rng &rng, std::int32_t population,
    const synth::counterparties::CounterpartyTargets &targets = {});

void finalizeAccountRegistry(pl::pipeline::Entities &entities);

} // namespace PhantomLedger::pipeline::stages::entities
