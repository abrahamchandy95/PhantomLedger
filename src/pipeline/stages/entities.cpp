#include "phantomledger/pipeline/stages/entities.hpp"

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/synth/accounts/assign.hpp"
#include "phantomledger/entities/synth/accounts/make.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/cards/seeds.hpp"
#include "phantomledger/entities/synth/counterparties/make.hpp"
#include "phantomledger/entities/synth/landlords/make.hpp"
#include "phantomledger/entities/synth/merchants/make.hpp"
#include "phantomledger/entities/synth/people/make.hpp"
#include "phantomledger/entities/synth/personas/make.hpp"
#include "phantomledger/entities/synth/pii/make.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

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

namespace {

void registerCreditCards(pl::pipeline::Entities &entities) {
  if (entities.creditCards.records.empty()) {
    return;
  }

  synth::accounts::assignOwners(
      entities.accounts, entities.creditCards.records,
      [](const entity::card::Terms &c) noexcept { return c.key; },
      [](const entity::card::Terms &c) noexcept { return c.owner; },
      entities.people.roster.count,
      /*external=*/false);
}

} // namespace

void finalizeAccountRegistry(pl::pipeline::Entities &entities) {
  using synth::accounts::addAccounts;
  using Key = entity::Key;
  namespace institutional = synth::products::institutional;

  registerCreditCards(entities);

  const std::array<Key, 3> systemKeys{
      pl::transfers::legit::ledger::bankFeeCollectionKey(),
      pl::transfers::legit::ledger::bankOdLocKey(),
      entity::makeKey(entity::Role::merchant, entity::Bank::external,
                      /*number=*/1ULL),
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
}

} // namespace PhantomLedger::pipeline::stages::entities
