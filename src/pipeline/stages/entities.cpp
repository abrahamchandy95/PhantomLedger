#include "phantomledger/pipeline/stages/entities.hpp"

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

#include <array>
#include <cassert>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::pipeline::stages::entities {

void validate(std::int32_t population) {
  if (population < 0) {
    throw std::invalid_argument("entities: population must be >= 0");
  }
}

void validate(
    const ::PhantomLedger::entities::synth::accounts::Sizing &sizing) {
  if (sizing.maxAccountsPerPerson < 1) {
    throw std::invalid_argument("entities: maxAccountsPerPerson must be >= 1");
  }
}

[[nodiscard]] IdentitySource
withDefaultStart(IdentitySource identity,
                 ::PhantomLedger::time::TimePoint fallbackStart) {
  if (identity.simStart == ::PhantomLedger::time::TimePoint{}) {
    identity.simStart = fallbackStart;
  }

  return identity;
}

[[nodiscard]] ::PhantomLedger::entities::synth::people::Pack
buildPeople(::PhantomLedger::random::Rng &rng, std::int32_t population,
            const ::PhantomLedger::entities::synth::people::Fraud &fraud) {
  validate(population);

  return ::PhantomLedger::entities::synth::people::make(rng, population, fraud);
}

[[nodiscard]] ::PhantomLedger::entities::synth::accounts::Pack buildAccounts(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::entities::synth::people::Pack &people,
    std::int32_t population,
    const ::PhantomLedger::entities::synth::accounts::Sizing &sizing) {
  validate(population);
  validate(sizing);

  return ::PhantomLedger::entities::synth::accounts::makePack(
      rng, people.roster, sizing.maxAccountsPerPerson);
}

[[nodiscard]] ::PhantomLedger::entities::synth::personas::Pack
buildPersonas(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::entities::synth::people::Pack &people,
              const ::PhantomLedger::entities::synth::personas::Mix &mix) {
  const std::uint64_t personasSeed = rng.nextU64();

  return ::PhantomLedger::entities::synth::personas::makePack(
      rng, people.roster.count, personasSeed, mix);
}

[[nodiscard]] ::PhantomLedger::entity::pii::Roster
buildPii(::PhantomLedger::random::Rng &rng,
         const ::PhantomLedger::entities::synth::personas::Pack &personas,
         IdentitySource identity) {
  assert(identity.pools != nullptr &&
         "buildPii: IdentitySource::pools must be set. main is the sole "
         "owner of the PoolSet pointer.");

  return ::PhantomLedger::entities::synth::pii::make(
      rng, personas.assignment, identity.simStart, identity.localeMix,
      *identity.pools);
}

[[nodiscard]] ::PhantomLedger::entity::merchant::Catalog buildMerchants(
    ::PhantomLedger::random::Rng &rng, std::int32_t population,
    const ::PhantomLedger::entities::synth::merchants::GenerationPlan &plan) {
  validate(population);

  return ::PhantomLedger::entities::synth::merchants::makeCatalog(
      rng, population, plan);
}

[[nodiscard]] ::PhantomLedger::entities::synth::landlords::Pack buildLandlords(
    ::PhantomLedger::random::Rng &rng, std::int32_t population,
    const ::PhantomLedger::entities::synth::landlords::GenerationPlan &plan) {
  validate(population);

  return ::PhantomLedger::entities::synth::landlords::makePack(rng, population,
                                                               plan);
}

[[nodiscard]] ::PhantomLedger::entity::card::Registry issueCreditCards(
    const ::PhantomLedger::entities::synth::personas::Pack &personas,
    const ::PhantomLedger::entities::synth::people::Pack &people,
    std::uint64_t topLevelSeed,
    const ::PhantomLedger::entities::synth::cards::IssuanceRules &issuance) {
  return ::PhantomLedger::entities::synth::cards::issue(
      ::PhantomLedger::entities::synth::cards::deriveCardSeed(topLevelSeed),
      personas.table, people.roster.count, issuance);
}

[[nodiscard]] ::PhantomLedger::entity::counterparty::Directory
buildCounterparties(
    ::PhantomLedger::random::Rng &rng, std::int32_t population,
    const ::PhantomLedger::entities::synth::counterparties::CounterpartyTargets
        &targets) {
  validate(population);

  return ::PhantomLedger::entities::synth::counterparties::make(rng, population,
                                                                targets);
}

void finalizeAccountRegistry(::PhantomLedger::pipeline::Entities &entities) {
  using ::PhantomLedger::entities::synth::accounts::addAccounts;
  using Key = ::PhantomLedger::entity::Key;
  namespace institutional =
      ::PhantomLedger::entities::synth::products::institutional;

  // 1. Bank-internal system accounts referenced by ChronoReplayAccumulator
  //    liquidity events (overdraft fees, OD line of credit).
  const std::array<Key, 3> systemKeys{
      ::PhantomLedger::transfers::legit::ledger::bankFeeCollectionKey(),
      ::PhantomLedger::transfers::legit::ledger::bankOdLocKey(),
      ::PhantomLedger::entity::makeKey(::PhantomLedger::entity::Role::merchant,
                                       ::PhantomLedger::entity::Bank::external,
                                       /*number=*/1ULL),
  };
  addAccounts(entities.accounts, std::span<const Key>{systemKeys},
              /*external=*/true);

  // 2. Institutional product counterparties.
  const std::array<Key, 7> institutionalKeys{
      institutional::mortgageLender(),  institutional::autoLender(),
      institutional::studentServicer(), institutional::irsTreasury(),
      institutional::autoCarrier(),     institutional::homeCarrier(),
      institutional::lifeCarrier(),
  };
  addAccounts(entities.accounts, std::span<const Key>{institutionalKeys},
              /*external=*/true);

  // 3. Merchant counterparty IDs — destinations for the spending pass.
  //    `entities.merchants` is now an `entity::merchant::Catalog` directly
  //    (not a Pack wrapper), so we read `.records` instead of
  //    `.catalog.records`.
  {
    std::vector<Key> keys;
    keys.reserve(entities.merchants.records.size());
    for (const auto &rec : entities.merchants.records) {
      keys.push_back(rec.counterpartyId);
    }
    addAccounts(entities.accounts, std::span<const Key>{keys},
                /*external=*/true);
  }

  // 4. Landlord account IDs — destinations for the rent pass.
  {
    std::vector<Key> keys;
    keys.reserve(entities.landlords.roster.records.size());
    for (const auto &rec : entities.landlords.roster.records) {
      keys.push_back(rec.accountId);
    }
    addAccounts(entities.accounts, std::span<const Key>{keys},
                /*external=*/true);
  }

  // 5. Counterparty directory: employers, clients, and external parties.
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
