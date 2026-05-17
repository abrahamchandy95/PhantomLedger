#pragma once

#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/accounts/business_owners.hpp"
#include "phantomledger/synth/accounts/pack.hpp"
#include "phantomledger/synth/accounts/sizing.hpp"
#include "phantomledger/synth/cards/issue.hpp"
#include "phantomledger/synth/counterparties/make.hpp"
#include "phantomledger/synth/landlords/make.hpp"
#include "phantomledger/synth/landlords/pack.hpp"
#include "phantomledger/synth/merchants/make.hpp"
#include "phantomledger/synth/personas/kinds.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/pii/identity.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::entities {

namespace pl = ::PhantomLedger;
namespace synth = pl::entities::synth;
namespace sy = pl::synth;
namespace entity = pl::entity;

struct EntitySynthesis {
  std::int32_t population;
  sy::pii::IdentityContext identity;
  synth::people::Fraud fraud{};
  sy::personas::Mix personaMix{};
  sy::accounts::Sizing accountsSizing{};
  sy::merchants::GenerationPlan merchants{};
  sy::landlords::GenerationPlan landlords{};
  sy::counterparties::CounterpartyTargets counterpartyTargets{};
  sy::cards::IssuanceRules cards{};

  sy::accounts::BusinessOwnerPlan businessOwners{};

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

[[nodiscard]] sy::pii::IdentityContext
defaultStart(sy::pii::IdentityContext identity,
             pl::time::TimePoint fallbackStart);

[[nodiscard]] synth::people::Pack
buildPeople(pl::random::Rng &rng, std::int32_t population,
            const synth::people::Fraud &fraud = {});

[[nodiscard]] sy::accounts::Pack
buildAccounts(pl::random::Rng &rng, const synth::people::Pack &people,
              std::int32_t population, const sy::accounts::Sizing &sizing = {});

[[nodiscard]] sy::personas::Pack
buildPersonas(pl::random::Rng &rng, const synth::people::Pack &people,
              const sy::personas::Mix &mix = {});

[[nodiscard]] entity::pii::Roster buildPii(pl::random::Rng &rng,
                                           const sy::personas::Pack &personas,
                                           sy::pii::IdentityContext identity);

[[nodiscard]] entity::merchant::Catalog
buildMerchants(pl::random::Rng &rng, std::int32_t population,
               const sy::merchants::GenerationPlan &plan = {});

[[nodiscard]] sy::landlords::Pack
buildLandlords(pl::random::Rng &rng, std::int32_t population,
               const sy::landlords::GenerationPlan &plan = {});

[[nodiscard]] entity::card::Registry
issueCreditCards(const sy::personas::Pack &personas,
                 const synth::people::Pack &people, std::uint64_t topLevelSeed,
                 const sy::cards::IssuanceRules &issuance = {});

[[nodiscard]] entity::counterparty::Directory buildCounterparties(
    pl::random::Rng &rng, std::int32_t population,
    const sy::counterparties::CounterpartyTargets &targets = {});

void finalizeAccountRegistry(pl::pipeline::Holdings &holdings,
                             const pl::pipeline::Counterparties &cps,
                             const pl::pipeline::People &people);

void synthesizeBusinessOwners(pl::pipeline::Holdings &holdings,
                              const pl::pipeline::People &people,
                              pl::random::Rng &rng,
                              const sy::accounts::BusinessOwnerPlan &plan = {});

} // namespace PhantomLedger::pipeline::stages::entities
