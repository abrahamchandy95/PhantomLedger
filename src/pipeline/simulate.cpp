#include "phantomledger/pipeline/simulate.hpp"

#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

#include <cstdint>
#include <span>
#include <utility>

namespace PhantomLedger::pipeline {

namespace {

namespace entityStage = stages::entities;
namespace legit = ::PhantomLedger::transfers::legit;
namespace fraud = ::PhantomLedger::transfers::fraud;
namespace credit_cards = ::PhantomLedger::transfers::credit_cards;
namespace clearing = ::PhantomLedger::clearing;
namespace tx_ns = ::PhantomLedger::transactions;

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

[[nodiscard]] std::size_t fraudMergedCapacity(std::size_t legitSize) noexcept {
  return legitSize + legitSize / 20;
}

void runTransferStage(SimulationResult &result,
                      SimulationPipeline::TransferStage &stage,
                      random::Rng &rng) {
  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &cps = result.counterparties;

  auto legitPayload = stage.buildLegit(rng, people, holdings, cps);
  auto productStream =
      stage.mergeProducts(rng, holdings, std::move(legitPayload.txns));
  auto candidate = stage.preFraudReplay(
      rng, *legitPayload.openingBook.initialBook, std::move(productStream));

  auto injector = stage.makeFraudInjector(rng, people, holdings);
  const std::span<const tx_ns::Transaction> candidateView{
      candidate.txns.data(), candidate.txns.size()};
  auto fraudOut =
      injector.inject(stage.legit().runScope().window, candidateView,
                      stages::transfers::FraudEmission::legitCounterparties(
                          legitPayload.counterparties));

  auto mergedTxns = std::move(fraudOut.txns);
  mergedTxns.reserve(fraudMergedCapacity(mergedTxns.size()));
  auto posted = stage.postFraudReplay(
      rng, *legitPayload.openingBook.initialBook, std::move(mergedTxns));

  validateTransactionAccounts(holdings.accounts.lookup, posted.txns);

  Transfers out{};
  out.legit = std::move(legitPayload);
  out.fraud.injectedCount = fraudOut.injectedCount;
  out.ledger.candidate.txns = std::move(candidate.txns);
  out.ledger.candidate.drops = std::move(candidate.drops);
  out.ledger.posted.txns = std::move(posted.txns);
  out.ledger.posted.book = std::move(posted.book);
  result.transfers = std::move(out);
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

void SimulationPipeline::buildEntities(SimulationResult &result) const {
  const auto &cfg = entities_;
  const auto identity = entityStage::defaultStart(cfg.identity, window_.start);

  auto &people = result.people;
  auto &holdings = result.holdings;
  auto &cps = result.counterparties;

  people.roster = entityStage::buildPeople(*rng_, cfg.population, cfg.fraud);
  holdings.accounts = entityStage::buildAccounts(
      *rng_, people.roster, cfg.population, cfg.accountsSizing);
  people.personas =
      entityStage::buildPersonas(*rng_, people.roster, cfg.personaMix);
  people.pii = entityStage::buildPii(*rng_, people.personas, identity);

  cps.merchants =
      entityStage::buildMerchants(*rng_, cfg.population, cfg.merchants);
  cps.landlords =
      entityStage::buildLandlords(*rng_, cfg.population, cfg.landlords);
  cps.counterparties = entityStage::buildCounterparties(
      *rng_, cfg.population, cfg.counterpartyTargets);

  holdings.creditCards = entityStage::issueCreditCards(
      people.personas, people.roster, seed_, cfg.cards);

  entityStage::finalizeAccountRegistry(holdings, cps, people);
  entityStage::synthesizeBusinessOwners(holdings, people, *rng_,
                                        cfg.businessOwners);
}

SimulationResult SimulationPipeline::run() const {
  SimulationResult out;

  buildEntities(out);

  products_.synthesize(out.people, out.holdings, window_);

  out.infra = infra_.build(*rng_, out.people, out.holdings, window_);

  auto stage = transfers_;
  configureTransferStage(stage, window_, seed_, entities_.fraud);
  stage.infra(out.infra);

  runTransferStage(out, stage, *rng_);

  return out;
}

SimulationResult simulate(random::Rng &rng, time::Window window,
                          SimulationPipeline::EntitySynthesis entities,
                          std::uint64_t seed) {
  return SimulationPipeline{rng, window, std::move(entities), seed}.run();
}

} // namespace PhantomLedger::pipeline
