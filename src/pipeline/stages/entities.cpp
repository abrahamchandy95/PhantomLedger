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

::PhantomLedger::pipeline::Entities build(::PhantomLedger::random::Rng &rng,
                                          const Inputs &in) {
  if (in.population < 0) {
    throw std::invalid_argument("entities::build: population must be >= 0");
  }
  if (in.piiPools == nullptr) {
    throw std::invalid_argument(
        "entities::build: piiPools is required (build via "
        "pii::buildLocalePool)");
  }

  ::PhantomLedger::pipeline::Entities out;

  out.people = ::PhantomLedger::entities::synth::people::make(
      rng, in.population, in.fraud);

  out.accounts = ::PhantomLedger::entities::synth::accounts::makePack(
      rng, out.people.roster, in.maxAccountsPerPerson);

  const std::uint64_t personasSeed = rng.nextU64();
  out.personas = ::PhantomLedger::entities::synth::personas::makePack(
      rng, out.people.roster.count, personasSeed, in.personaMix);

  out.pii = ::PhantomLedger::entities::synth::pii::make(
      rng, out.personas.assignment, in.simStart, in.localeMix, *in.piiPools);

  out.merchants = ::PhantomLedger::entities::synth::merchants::makePack(
      rng, in.population, in.merchantsCfg);

  out.landlords = ::PhantomLedger::entities::synth::landlords::makePack(
      rng, in.population, in.landlordsCfg);

  out.creditCards = ::PhantomLedger::entities::synth::cards::issue(
      cardsIssuanceBase(in.cardsBaseSeed), out.personas.table,
      out.people.roster.count, in.cardsPolicy);

  out.counterpartyPools =
      ::PhantomLedger::entities::synth::counterparties::makePool(
          rng, in.population, in.counterpartiesCfg);

  ::PhantomLedger::entities::synth::products::Inputs productsIn{};
  productsIn.window = in.window;
  productsIn.personas = &out.personas;
  productsIn.creditCards = &out.creditCards;
  productsIn.mortgageCfg = in.mortgageCfg;
  productsIn.autoLoanCfg = in.autoLoanCfg;
  productsIn.studentLoanCfg = in.studentLoanCfg;
  productsIn.taxCfg = in.taxCfg;
  productsIn.insuranceCfg = in.insuranceCfg;
  productsIn.baseSeed = in.productsBaseSeed;

  out.portfolios =
      ::PhantomLedger::entities::synth::products::build(rng, productsIn);

  return out;
}

} // namespace PhantomLedger::pipeline::stages::entities
