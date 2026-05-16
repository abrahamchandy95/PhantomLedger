#include "phantomledger/pipeline/simulate.hpp"

#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

#include <cstdint>
#include <utility>

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = stages::entities;
namespace legit = ::PhantomLedger::transfers::legit;
namespace fraud = ::PhantomLedger::transfers::fraud;
namespace credit_cards = ::PhantomLedger::transfers::credit_cards;
namespace clearing = ::PhantomLedger::clearing;

using SynthFraud = ::PhantomLedger::entities::synth::people::Fraud;

[[nodiscard]] auto resolveRunScope(legit::LegitAssembly::RunScope scope,
                                   time::Window fallbackWindow,
                                   std::uint64_t fallbackSeed) noexcept
    -> legit::LegitAssembly::RunScope {
  if (scope.window.days == 0) {
    scope.window = fallbackWindow;
  }
  if (scope.seed == 0) {
    scope.seed = fallbackSeed;
  }
  return scope;
}

void configureTransferStage(SimulationPipeline::TransferStage &stage,
                            time::Window window, std::uint64_t seed,
                            const SynthFraud &fraudProfile) noexcept {
  stage.legit().runScope(
      resolveRunScope(stage.legit().runScope(), window, seed));
  stage.legit().openingBalanceRules(&clearing::kDefaultBalanceRules);
  stage.legit().creditLifecycle(&credit_cards::kDefaultLifecycleRules);

  stage.fraud().profile(&fraudProfile);
  stage.fraud().behavior(&fraud::kDefaultBehavior);
}

/// Entity construction now writes into the three SRP-split sub-domains of
/// `SimulationResult` directly. We pass `result` by reference rather than
/// returning a god-struct: the caller already owns the destination, and
/// it keeps the param list short. The sub-domains are still independent
/// types — there is no monolithic Entities here.
void buildEntities(SimulationResult &result, random::Rng &rng,
                   time::Window window,
                   const SimulationPipeline::EntitySynthesis &cfg,
                   std::uint64_t seed) {
  const auto identity = entityStage::defaultStart(cfg.identity, window.start);

  auto &people = result.people;
  auto &holdings = result.holdings;
  auto &cps = result.counterparties;

  people.roster = entityStage::buildPeople(rng, cfg.population, cfg.fraud);
  holdings.accounts = entityStage::buildAccounts(
      rng, people.roster, cfg.population, cfg.accountsSizing);
  people.personas =
      entityStage::buildPersonas(rng, people.roster, cfg.personaMix);
  people.pii = entityStage::buildPii(rng, people.personas, identity);

  cps.merchants =
      entityStage::buildMerchants(rng, cfg.population, cfg.merchants);
  cps.landlords =
      entityStage::buildLandlords(rng, cfg.population, cfg.landlords);
  cps.counterparties = entityStage::buildCounterparties(
      rng, cfg.population, cfg.counterpartyTargets);

  holdings.creditCards = entityStage::issueCreditCards(
      people.personas, people.roster, seed, cfg.cards);

  // finalizeAccountRegistry / synthesizeBusinessOwners take only the
  // narrow sub-domains they actually use (holdings mutated, people+cps
  // read).
  entityStage::finalizeAccountRegistry(holdings, cps, people);
  entityStage::synthesizeBusinessOwners(holdings, people, rng,
                                        cfg.businessOwners);
}

} // namespace

SimulationPipeline::SimulationPipeline(random::Rng &rng, time::Window window,
                                       EntitySynthesis entities,
                                       std::uint64_t seed)
    : rng_(&rng), window_(window), seed_(seed), entities_(std::move(entities)) {
}

SimulationPipeline::InfraStage &SimulationPipeline::infraStage() noexcept {
  return infra_;
}

const SimulationPipeline::InfraStage &
SimulationPipeline::infraStage() const noexcept {
  return infra_;
}

SimulationPipeline::ProductSynthesis &SimulationPipeline::products() noexcept {
  return products_;
}

const SimulationPipeline::ProductSynthesis &
SimulationPipeline::products() const noexcept {
  return products_;
}

SimulationPipeline::TransferStage &
SimulationPipeline::transferStage() noexcept {
  return transfers_;
}

const SimulationPipeline::TransferStage &
SimulationPipeline::transferStage() const noexcept {
  return transfers_;
}

SimulationResult SimulationPipeline::run() const {
  SimulationResult out;

  // 1. Entity layer — populates out.people / out.holdings / out.counterparties.
  buildEntities(out, *rng_, window_, entities_, seed_);

  // 2. Product synthesis — adds loans/obligations/insurance into
  //    out.holdings.portfolios, driven by personas in out.people.
  products_.synthesize(out.people, out.holdings, window_);

  // 3. Infrastructure — needs People (rings/devices/ips) + Holdings
  //    (account-to-owner map for the router).
  out.infra = infra_.build(*rng_, out.people, out.holdings, window_);

  // 4. Transfer stage — genuinely cross-domain. Takes the three SRP
  //    sub-domains as separate `const T&` parameters; this is the
  //    integration point of the entity layer, so 5 total params (rng +
  //    3 sub-domains + infra) is appropriate.
  auto stage = transfers_;
  configureTransferStage(stage, window_, seed_, entities_.fraud);
  out.transfers = stage.build(*rng_, out.people, out.holdings,
                              out.counterparties, out.infra);

  return out;
}

SimulationResult simulate(random::Rng &rng, time::Window window,
                          SimulationPipeline::EntitySynthesis entities,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, std::move(entities), seed}.run();
}

} // namespace PhantomLedger::pipeline
