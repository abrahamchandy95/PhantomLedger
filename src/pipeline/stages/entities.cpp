#include "phantomledger/pipeline/stages/entities.hpp"

#include "phantomledger/entities/synth/accounts/make.hpp"
#include "phantomledger/entities/synth/cards/issue.hpp"
#include "phantomledger/entities/synth/counterparties/make.hpp"
#include "phantomledger/entities/synth/landlords/make.hpp"
#include "phantomledger/entities/synth/merchants/make.hpp"
#include "phantomledger/entities/synth/people/make.hpp"
#include "phantomledger/entities/synth/personas/make.hpp"
#include "phantomledger/entities/synth/pii/make.hpp"
#include "phantomledger/entities/synth/products/build.hpp"

#include <stdexcept>

namespace PhantomLedger::pipeline::stages::entities {

namespace {

[[nodiscard]] std::uint64_t cardsIssuanceBase(std::uint64_t userBaseSeed) {
  return userBaseSeed;
}

} // namespace

::PhantomLedger::pipeline::Entities
build(::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
      IdentitySource identity, PopulationPlan population,
      ::PhantomLedger::entities::synth::people::Fraud fraud,
      ::PhantomLedger::entities::synth::personas::Mix personaMix,
      ::PhantomLedger::entities::synth::merchants::Config merchants,
      ::PhantomLedger::entities::synth::landlords::Config landlords,
      ::PhantomLedger::entities::synth::counterparties::Config counterparties,
      ::PhantomLedger::entities::synth::cards::IssuanceRules cardIssuance,
      Seeds seeds,
      ::PhantomLedger::entities::synth::products::MortgageTerms mortgage,
      ::PhantomLedger::entities::synth::products::AutoLoanTerms autoLoan,
      ::PhantomLedger::entities::synth::products::StudentLoanTerms studentLoan,
      ::PhantomLedger::entities::synth::products::TaxTerms tax,
      ::PhantomLedger::entities::synth::products::InsuranceTerms insurance) {
  if (population.count < 0) {
    throw std::invalid_argument("entities::build: population must be >= 0");
  }

  if (population.maxAccountsPerPerson < 1) {
    throw std::invalid_argument(
        "entities::build: maxAccountsPerPerson must be >= 1");
  }

  if (identity.simStart == ::PhantomLedger::time::TimePoint{}) {
    identity.simStart = window.start;
  }

  ::PhantomLedger::pipeline::Entities out;

  out.people = ::PhantomLedger::entities::synth::people::make(
      rng, population.count, fraud);

  out.accounts = ::PhantomLedger::entities::synth::accounts::makePack(
      rng, out.people.roster, population.maxAccountsPerPerson);

  const std::uint64_t personasSeed = rng.nextU64();
  out.personas = ::PhantomLedger::entities::synth::personas::makePack(
      rng, out.people.roster.count, personasSeed, personaMix);

  out.pii = ::PhantomLedger::entities::synth::pii::make(
      rng, out.personas.assignment, identity.simStart, identity.localeMix,
      identity.pools);

  out.merchants = ::PhantomLedger::entities::synth::merchants::makePack(
      rng, population.count, merchants);

  out.landlords = ::PhantomLedger::entities::synth::landlords::makePack(
      rng, population.count, landlords);

  out.creditCards = ::PhantomLedger::entities::synth::cards::issue(
      cardsIssuanceBase(seeds.cardIssuance), out.personas.table,
      out.people.roster.count, cardIssuance);

  out.counterpartyPools =
      ::PhantomLedger::entities::synth::counterparties::makePool(
          rng, population.count, counterparties);

  out.portfolios = ::PhantomLedger::entities::synth::products::build(
      rng, window, out.personas, out.creditCards, seeds.products, mortgage,
      autoLoan, studentLoan, tax, insurance);

  return out;
}

} // namespace PhantomLedger::pipeline::stages::entities
