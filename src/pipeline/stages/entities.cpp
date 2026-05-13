#include "phantomledger/pipeline/stages/entities.hpp"

#include "phantomledger/entities/synth/accounts/assign.hpp"
#include "phantomledger/entities/synth/accounts/business_owners.hpp"
#include "phantomledger/entities/synth/accounts/make.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/cards/seeds.hpp"
#include "phantomledger/entities/synth/counterparties/make.hpp"
#include "phantomledger/entities/synth/family/pick.hpp"
#include "phantomledger/entities/synth/landlords/make.hpp"
#include "phantomledger/entities/synth/merchants/make.hpp"
#include "phantomledger/entities/synth/people/make.hpp"
#include "phantomledger/entities/synth/personas/make.hpp"
#include "phantomledger/entities/synth/pii/make.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/channels/government/keys.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <array>
#include <cassert>
#include <span>
#include <vector>

namespace PhantomLedger::pipeline::stages::entities {

[[nodiscard]] synth::pii::IdentityContext
defaultStart(synth::pii::IdentityContext identity,
             pl::time::TimePoint fallbackStart) {
  if (identity.simStart == pl::time::TimePoint{}) {
    identity.simStart = fallbackStart;
  }

  return identity;
}

[[nodiscard]] synth::people::Pack
buildPeople(pl::random::Rng &rng, std::int32_t population,
            const synth::people::Fraud &fraud) {
  pl::primitives::validate::nonNegative("population", population);

  return synth::people::make(rng, population, fraud);
}

[[nodiscard]] synth::accounts::Pack
buildAccounts(pl::random::Rng &rng, const synth::people::Pack &people,
              std::int32_t population, const synth::accounts::Sizing &sizing) {
  pl::primitives::validate::nonNegative("population", population);

  pl::primitives::validate::require(sizing);

  return synth::accounts::makePack(rng, people.roster,
                                   sizing.maxAccountsPerPerson);
}

[[nodiscard]] synth::personas::Pack
buildPersonas(pl::random::Rng &rng, const synth::people::Pack &people,
              const synth::personas::Mix &mix) {
  const std::uint64_t personasSeed = rng.nextU64();

  return synth::personas::makePack(rng, people.roster.count, personasSeed, mix);
}

[[nodiscard]] entity::pii::Roster
buildPii(pl::random::Rng &rng, const synth::personas::Pack &personas,
         synth::pii::IdentityContext identity) {
  assert(identity.pools != nullptr &&
         "buildPii: IdentityContext::pools must be set. main is the sole "
         "owner of the PoolSet pointer.");

  return synth::pii::make(rng, personas.assignment, identity);
}

[[nodiscard]] entity::merchant::Catalog
buildMerchants(pl::random::Rng &rng, std::int32_t population,
               const synth::merchants::GenerationPlan &plan) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(plan);
  return synth::merchants::makeCatalog(rng, population, plan);
}

[[nodiscard]] synth::landlords::Pack
buildLandlords(pl::random::Rng &rng, std::int32_t population,
               const synth::landlords::GenerationPlan &plan) {
  pl::primitives::validate::nonNegative("population", population);

  pl::primitives::validate::require(plan);

  return synth::landlords::makePack(rng, population, plan);
}

[[nodiscard]] entity::card::Registry
issueCreditCards(const synth::personas::Pack &personas,
                 const synth::people::Pack &people, std::uint64_t topLevelSeed,
                 const synth::cards::IssuanceRules &issuance) {
  return synth::cards::issue(synth::cards::deriveCardSeed(topLevelSeed),
                             personas.table, people.roster.count, issuance);
}

[[nodiscard]] entity::counterparty::Directory
buildCounterparties(pl::random::Rng &rng, std::int32_t population,
                    const synth::counterparties::CounterpartyTargets &targets) {
  pl::primitives::validate::nonNegative("population", population);

  pl::primitives::validate::require(targets);

  return synth::counterparties::make(rng, population, targets);
}

void finalizeAccountRegistry(pl::pipeline::Entities &entities) {
  using synth::accounts::addAccounts;
  using Key = entity::Key;
  namespace institutional = synth::products::institutional;
  namespace gov = pl::transfers::government;
  namespace family_synth = pl::entities::synth::family;
  namespace family_rt = pl::transfers::legit::routines::family;

  const std::array<Key, 5> systemKeys{
      pl::transfers::legit::ledger::bankFeeCollectionKey(),
      pl::transfers::legit::ledger::bankOdLocKey(),
      entity::makeKey(entity::Role::merchant, entity::Bank::external,
                      /*number=*/1ULL),
      gov::ssaCounterpartyKey(),
      gov::disabilityCounterpartyKey(),
  };
  addAccounts(entities.accounts, std::span<const Key>{systemKeys},
              /*external=*/true);

  const std::array<Key, 7> institutionalKeys{
      institutional::mortgageLender(),  institutional::autoLender(),
      institutional::studentServicer(), institutional::irsTreasury(),
      institutional::autoCarrier(),     institutional::homeCarrier(),
      institutional::lifeCarrier(),
  };
  addAccounts(entities.accounts, std::span<const Key>{institutionalKeys},
              /*external=*/true);

  {
    std::vector<Key> keys;
    keys.reserve(entities.merchants.records.size());
    for (const auto &rec : entities.merchants.records) {
      keys.push_back(rec.counterpartyId);
    }
    addAccounts(entities.accounts, std::span<const Key>{keys},
                /*external=*/true);
  }

  {
    std::vector<Key> keys;
    keys.reserve(entities.landlords.roster.records.size());
    for (const auto &rec : entities.landlords.roster.records) {
      keys.push_back(rec.accountId);
    }
    addAccounts(entities.accounts, std::span<const Key>{keys},
                /*external=*/true);
  }

  // Counterparty directory: employers, clients, and external parties.
  const auto &cps = entities.counterparties;
  addAccounts(entities.accounts,
              std::span<const Key>{cps.employers.accounts.all},
              /*external=*/true);
  addAccounts(entities.accounts, std::span<const Key>{cps.clients.accounts.all},
              /*external=*/true);
  addAccounts(entities.accounts, std::span<const Key>{cps.external.platforms},
              /*external=*/true);
  addAccounts(entities.accounts, std::span<const Key>{cps.external.processors},
              /*external=*/true);
  addAccounts(entities.accounts,
              std::span<const Key>{cps.external.ownerBusinesses},
              /*external=*/true);
  addAccounts(entities.accounts, std::span<const Key>{cps.external.brokerages},
              /*external=*/true);

  {
    std::vector<Key> keys;
    keys.reserve(entities.creditCards.records.size());
    for (const auto &rec : entities.creditCards.records) {
      keys.push_back(rec.key);
    }
    addAccounts(entities.accounts, std::span<const Key>{keys},
                /*external=*/false);
  }

  {
    constexpr double externalP =
        family_rt::kDefaultCounterpartyRouting.externalP;
    const auto perPerson = family_synth::plan(
        entities.people.roster, entities.accounts.ownership, externalP);

    std::vector<Key> keys;
    keys.reserve(perPerson.size());
    for (const auto &personKeys : perPerson) {
      for (const auto &key : personKeys) {
        keys.push_back(key);
      }
    }
    addAccounts(entities.accounts, std::span<const Key>{keys},
                /*external=*/true);
  }
}

void synthesizeBusinessOwners(pl::pipeline::Entities &entities,
                              pl::random::Rng &rng,
                              const synth::accounts::BusinessOwnerPlan &plan) {
  synth::accounts::assignBusinessOwners(entities.accounts,
                                        entities.people.roster, rng, plan);
}

} // namespace PhantomLedger::pipeline::stages::entities
