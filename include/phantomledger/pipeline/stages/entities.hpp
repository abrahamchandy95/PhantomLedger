#pragma once

#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entities/synth/accounts/business_owners.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"
#include "phantomledger/entities/synth/accounts/sizing.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/counterparties/make.hpp"
#include "phantomledger/entities/synth/landlords/make.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/entities/synth/personas/kinds.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entities/synth/pii/identity.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/merchants/make.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::entities {

namespace pl = ::PhantomLedger;
namespace synth = pl::entities::synth;
namespace sy = pl::synth;
namespace entity = pl::entity;

struct EntitySynthesis {
  std::int32_t population;
  synth::pii::IdentityContext identity;
  synth::people::Fraud fraud{};
  synth::personas::Mix personaMix{};
  synth::accounts::Sizing accountsSizing{};
  sy::merchants::GenerationPlan merchants{};
  synth::landlords::GenerationPlan landlords{};
  synth::counterparties::CounterpartyTargets counterpartyTargets{};
  synth::cards::IssuanceRules cards{};

  synth::accounts::BusinessOwnerPlan businessOwners{};

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

[[nodiscard]] synth::pii::IdentityContext
defaultStart(synth::pii::IdentityContext identity,
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
         synth::pii::IdentityContext identity);

[[nodiscard]] entity::merchant::Catalog
buildMerchants(pl::random::Rng &rng, std::int32_t population,
               const sy::merchants::GenerationPlan &plan = {});

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

void finalizeAccountRegistry(pl::pipeline::Holdings &holdings,
                             const pl::pipeline::Counterparties &cps,
                             const pl::pipeline::People &people);

void synthesizeBusinessOwners(
    pl::pipeline::Holdings &holdings, const pl::pipeline::People &people,
    pl::random::Rng &rng, const synth::accounts::BusinessOwnerPlan &plan = {});

} // namespace PhantomLedger::pipeline::stages::entities
