#include "phantomledger/pipeline/stages/entities.hpp"

#include "phantomledger/entities/synth/accounts/assign.hpp"
#include "phantomledger/entities/synth/accounts/make.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/counterparties/make.hpp"
#include "phantomledger/entities/synth/landlords/make.hpp"
#include "phantomledger/entities/synth/merchants/make.hpp"
#include "phantomledger/entities/synth/people/make.hpp"
#include "phantomledger/entities/synth/personas/make.hpp"
#include "phantomledger/entities/synth/pii/make.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"
#include "phantomledger/pipeline/state.hpp"

#include <array>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::pipeline::stages::entities {

namespace {

[[nodiscard]] std::uint64_t cardsIssuanceBase(std::uint64_t userBaseSeed) {
  return userBaseSeed;
}

} // namespace

void validate(PopulationSizing population) {
  if (population.count < 0) {
    throw std::invalid_argument("entities: population must be >= 0");
  }

  if (population.maxAccountsPerPerson < 1) {
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
buildPeople(::PhantomLedger::random::Rng &rng, PopulationSizing population,
            const ::PhantomLedger::entities::synth::people::Fraud &fraud) {
  validate(population);

  return ::PhantomLedger::entities::synth::people::make(rng, population.count,
                                                        fraud);
}

[[nodiscard]] ::PhantomLedger::entities::synth::accounts::Pack
buildAccounts(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::entities::synth::people::Pack &people,
              PopulationSizing population) {
  validate(population);

  return ::PhantomLedger::entities::synth::accounts::makePack(
      rng, people.roster, population.maxAccountsPerPerson);
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
  return ::PhantomLedger::entities::synth::pii::make(
      rng, personas.assignment, identity.simStart, identity.localeMix,
      identity.pools);
}

[[nodiscard]] ::PhantomLedger::entities::synth::merchants::Pack buildMerchants(
    ::PhantomLedger::random::Rng &rng, PopulationSizing population,
    const ::PhantomLedger::entities::synth::merchants::GenerationPlan &plan) {
  validate(population);

  return ::PhantomLedger::entities::synth::merchants::makePack(
      rng, population.count, plan);
}

[[nodiscard]] ::PhantomLedger::entities::synth::landlords::Pack buildLandlords(
    ::PhantomLedger::random::Rng &rng, PopulationSizing population,
    const ::PhantomLedger::entities::synth::landlords::GenerationPlan &plan) {
  validate(population);

  return ::PhantomLedger::entities::synth::landlords::makePack(
      rng, population.count, plan);
}

[[nodiscard]] ::PhantomLedger::entity::card::Registry issueCreditCards(
    const ::PhantomLedger::entities::synth::personas::Pack &personas,
    const ::PhantomLedger::entities::synth::people::Pack &people,
    const CardSynthesis &cards) {
  return ::PhantomLedger::entities::synth::cards::issue(
      cardsIssuanceBase(cards.seed), personas.table, people.roster.count,
      cards.issuance);
}

[[nodiscard]] ::PhantomLedger::entity::counterparty::Directory
buildCounterparties(
    ::PhantomLedger::random::Rng &rng, PopulationSizing population,
    const ::PhantomLedger::entities::synth::counterparties::CounterpartyTargets
        &targets) {
  validate(population);

  return ::PhantomLedger::entities::synth::counterparties::make(
      rng, population.count, targets);
}

void finalizeAccountRegistry(::PhantomLedger::pipeline::Entities &entities) {
  using ::PhantomLedger::entities::synth::accounts::addAccounts;
  using Key = ::PhantomLedger::entity::Key;
  namespace institutional =
      ::PhantomLedger::entities::synth::products::institutional;

  // 1. Bank-internal system accounts referenced by ChronoReplayAccumulator
  //    liquidity events (overdraft fees, OD line of credit). These exist
  //    independently of population and counterparty configuration.
  //
  //    The "external unknown merchant" placeholder key (merchant/external/1)
  //    is appended here too: spending::routing::emitExternal() and
  //    RunPlanner::build() both hard-code this Key as the destination /
  //    ledger lookup for Slot::externalUnknown spending events. It is a
  //    fixed placeholder, not a catalog entry, so it must be registered
  //    independently of merchants::Pack.
  const std::array<Key, 3> systemKeys{
      ::PhantomLedger::transfers::legit::ledger::bankFeeCollectionKey(),
      ::PhantomLedger::transfers::legit::ledger::bankOdLocKey(),
      ::PhantomLedger::entity::makeKey(::PhantomLedger::entity::Role::merchant,
                                       ::PhantomLedger::entity::Bank::external,
                                       /*number=*/1ULL),
  };
  addAccounts(entities.accounts, std::span<const Key>{systemKeys},
              /*external=*/true);

  // 2. Institutional product counterparties — destinations for mortgage,
  //    auto-loan, student-loan, tax, and the three insurance carrier
  //    obligation streams. These are constexpr keys defined alongside the
  //    product synthesizers; obligations target them directly without
  //    going through the counterparty Directory.
  const std::array<Key, 7> institutionalKeys{
      institutional::mortgageLender(),  institutional::autoLender(),
      institutional::studentServicer(), institutional::irsTreasury(),
      institutional::autoCarrier(),     institutional::homeCarrier(),
      institutional::lifeCarrier(),
  };
  addAccounts(entities.accounts, std::span<const Key>{institutionalKeys},
              /*external=*/true);

  // 3. Merchant counterparty IDs — destinations for the spending pass.
  {
    std::vector<Key> keys;
    keys.reserve(entities.merchants.catalog.records.size());
    for (const auto &rec : entities.merchants.catalog.records) {
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

  // 5. Counterparty directory: employers, client payers, and external
  //    parties (platforms, processors, owner-businesses, brokerages).
  //    BankSplit::all is the union of internal+external, which is what we
  //    want to cover regardless of which side of the bank they sit on.
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
