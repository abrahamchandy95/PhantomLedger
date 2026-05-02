#include "phantomledger/pipeline/stages/entities.hpp"

#include "phantomledger/entities/synth/accounts/make.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/counterparties/make.hpp"
#include "phantomledger/entities/synth/landlords/make.hpp"
#include "phantomledger/entities/synth/merchants/make.hpp"
#include "phantomledger/entities/synth/people/make.hpp"
#include "phantomledger/entities/synth/personas/make.hpp"
#include "phantomledger/entities/synth/pii/make.hpp"

#include <stdexcept>

namespace PhantomLedger::pipeline::stages::entities {

namespace {

[[nodiscard]] std::uint64_t cardsIssuanceBase(std::uint64_t userBaseSeed) {
  return userBaseSeed;
}

} // namespace

void validate(PopulationPlan population) {
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
buildPeople(::PhantomLedger::random::Rng &rng, PopulationPlan population,
            const ::PhantomLedger::entities::synth::people::Fraud &fraud) {
  validate(population);

  return ::PhantomLedger::entities::synth::people::make(rng, population.count,
                                                        fraud);
}

[[nodiscard]] ::PhantomLedger::entities::synth::accounts::Pack
buildAccounts(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::entities::synth::people::Pack &people,
              PopulationPlan population) {
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
    ::PhantomLedger::random::Rng &rng, PopulationPlan population,
    const ::PhantomLedger::entities::synth::merchants::Config &config) {
  validate(population);

  return ::PhantomLedger::entities::synth::merchants::makePack(
      rng, population.count, config);
}

[[nodiscard]] ::PhantomLedger::entities::synth::landlords::Pack buildLandlords(
    ::PhantomLedger::random::Rng &rng, PopulationPlan population,
    const ::PhantomLedger::entities::synth::landlords::Config &config) {
  validate(population);

  return ::PhantomLedger::entities::synth::landlords::makePack(
      rng, population.count, config);
}

[[nodiscard]] ::PhantomLedger::entity::card::Registry issueCreditCards(
    const ::PhantomLedger::entities::synth::personas::Pack &personas,
    const ::PhantomLedger::entities::synth::people::Pack &people, Seeds seeds,
    const ::PhantomLedger::entities::synth::cards::IssuanceRules &rules) {
  return ::PhantomLedger::entities::synth::cards::issue(
      cardsIssuanceBase(seeds.cardIssuance), personas.table,
      people.roster.count, rules);
}

[[nodiscard]] ::PhantomLedger::entity::counterparty::Pool buildCounterparties(
    ::PhantomLedger::random::Rng &rng, PopulationPlan population,
    const ::PhantomLedger::entities::synth::counterparties::Config &config) {
  validate(population);

  return ::PhantomLedger::entities::synth::counterparties::makePool(
      rng, population.count, config);
}

} // namespace PhantomLedger::pipeline::stages::entities
