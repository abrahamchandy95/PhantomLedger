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
#include "phantomledger/entities/synth/merchants/make.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/entities/synth/personas/kinds.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entities/synth/pii/identity.hpp"
#include "phantomledger/primitives/random/rng.hpp"
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

struct EntitySynthesis {
  std::int32_t population;
  synth::pii::IdentityContext identity;
  synth::people::Fraud fraud{};
  synth::personas::Mix personaMix{};
  synth::accounts::Sizing accountsSizing{};
  synth::merchants::GenerationPlan merchants{};
  synth::landlords::GenerationPlan landlords{};
  synth::counterparties::CounterpartyTargets counterpartyTargets{};
  synth::cards::IssuanceRules cards{};

  // Small-business-owner synthesis. Controls how many internal
  // (Role::business, Bank::internal) accounts get added to the registry
  // with Person owners — populates the AML exporter's Business / SIGNER_OF
  // / BENEFICIAL_OWNER_OF / CONTROLS / BUSINESS_OWNS_ACCOUNT outputs.
  // Default (probability 0.08) matches US small-business demographics;
  // set to 0.0 in the plan to disable.
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

// Adds internal small-business accounts to the registry, owned by a
// fraction of the existing Person population. Must run AFTER
// finalizeAccountRegistry so the per-person ownership index can be
// rebuilt in one pass over the final registry shape.
//
// Producing these accounts here (rather than at exporter time) means
// downstream consumers see a realistic, consistent account population:
// the same business accounts that show up in AML's Business vertices
// also receive transactions, payroll, and ledger postings via the
// standard transfer routines, since those routines walk the account
// registry without caring about the role flag.
void synthesizeBusinessOwners(
    pl::pipeline::Entities &entities, pl::random::Rng &rng,
    const synth::accounts::BusinessOwnerPlan &plan = {});

} // namespace PhantomLedger::pipeline::stages::entities
