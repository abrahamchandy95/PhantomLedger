#include "phantomledger/pipeline/stages/transfers/orchestrator.hpp"

#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/transactions/factory.hpp"

#include <stdexcept>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

using Transaction = ::PhantomLedger::transactions::Transaction;

}

legit::LegitAssembly &TransferStage::legit() noexcept { return legit_; }

const legit::LegitAssembly &TransferStage::legit() const noexcept {
  return legit_;
}

ProductReplay &TransferStage::products() noexcept { return products_; }

const ProductReplay &TransferStage::products() const noexcept {
  return products_;
}

LedgerReplay &TransferStage::ledger() noexcept { return ledger_; }

const LedgerReplay &TransferStage::ledger() const noexcept { return ledger_; }

FraudEmission &TransferStage::fraud() noexcept { return fraud_; }

const FraudEmission &TransferStage::fraud() const noexcept { return fraud_; }

TransferStage &TransferStage::infra(const pipeline::Infra &value) {
  infra_ = &value;
  // Pristine-router snapshot for product routing; see the member comment.
  productRouter_ = value.router;
  return *this;
}

TransferStage &TransferStage::obligationSynthesis(
    const stages::products::ObligationSynthesis &value) noexcept {
  obligationSynthesis_ = &value;
  return *this;
}

chunk::Strategy TransferStage::settlementChunking() const noexcept {
  return settlementChunking_;
}

TransferStage &
TransferStage::settlementChunking(chunk::Strategy value) noexcept {
  settlementChunking_ = value;
  return *this;
}

legit::LegitAssembly::WorldInputs
legitWorldInputs(const pipeline::People &people,
                 const pipeline::Holdings &holdings,
                 const pipeline::Counterparties &cps) noexcept {
  return legit::LegitAssembly::WorldInputs{
      .personas = &people.personas,
      .populationCount = people.roster.roster.count,
      .accounts = &holdings.accounts,
      .creditCards = &holdings.creditCards,
      .portfolios = &holdings.portfolios,
      .counterparties = &cps.counterparties,
      .landlords = &cps.landlords,
      .merchants = &cps.merchants,
      /* The compact per-person home carrier: a span into People::homeAreas,
       * which outlives the builder. Reaches the spending market via
       * RoutineResources → CensusSource, so BOTH the windowed and monolith
       * engines select from the same home areas. */
      .homeAreas = people.homeAreas,
      /* The home-area HISTORY behind the snapshot, so both engines refresh
       * from one schedule. */
      .relocation = &people.relocation,
  };
}

namespace {

[[nodiscard]] const pipeline::Infra &requireInfra(const pipeline::Infra *p) {
  if (p == nullptr) {
    throw std::runtime_error("transfers::TransferStage: infra not set; call "
                             ".infra(out.infra) before "
                             "any phase method");
  }
  return *p;
}

[[nodiscard]] const stages::products::ObligationSynthesis &
requireObligationSynthesis(const stages::products::ObligationSynthesis *p) {
  if (p == nullptr) {
    throw std::runtime_error(
        "transfers::TransferStage: obligation synthesis not set; call "
        ".obligationSynthesis(pipeline products) before generating product "
        "rows — RAM R2.2.1c: the emitters replay it for the whole-window "
        "obligation stream");
  }
  return *p;
}

} // namespace

legit::ledger::LegitTransferResult
TransferStage::buildLegit(::PhantomLedger::random::Rng &rng,
                          const pipeline::People &people,
                          const pipeline::Holdings &holdings,
                          const pipeline::Counterparties &cps) const {
  legit_.validate();
  const auto &infra = requireInfra(infra_);
  auto builder = legit_.builder(rng, legitWorldInputs(people, holdings, cps));
  builder.router(infra.router);
  auto result = builder.build();
  if (!result.openingBook.hasInitialBook()) {
    throw std::runtime_error(
        "transfers::TransferStage::buildLegit: legit builder produced no "
        "initial book");
  }
  ::PhantomLedger::pipeline::validateTransactionAccounts(
      holdings.accounts.lookup, result.txns.replaySortedTxns);
  return result;
}

std::vector<Transaction>
TransferStage::mergeProducts(::PhantomLedger::random::Rng &rng,
                             const pipeline::People &people,
                             const pipeline::Holdings &holdings,
                             legit::ledger::LegitTxnStreams streams) const {
  const auto &infra = requireInfra(infra_);
  const auto &synthesis = requireObligationSynthesis(obligationSynthesis_);
  const auto scope = legit_.runScope();
  const auto primaryAccountsByPerson = primaryAccounts(holdings);

  // Products route from the pristine snapshot, not the live shared router;
  // see the productRouter_ member comment in orchestrator.hpp.
  ::PhantomLedger::transactions::Factory productTxf{rng, &productRouter_,
                                                    &infra.ringInfra};
  ProductTxnEmitter productEmitter{scope.window, scope.seed, rng,
                                   productTxf,   people,     synthesis};

  return products_.merge(productEmitter, holdings, primaryAccountsByPerson,
                         streams);
}

fraud_ns::Injector
TransferStage::makeFraudInjector(::PhantomLedger::random::Rng &rng,
                                 const pipeline::People &people,
                                 const pipeline::Holdings &holdings) const {
  const auto &infra = requireInfra(infra_);
  return fraud_ns::Injector{
      fraud_ns::InjectorServices{
          .rng = rng,
          .router = &infra.router,
          .ringInfra = &infra.ringInfra,
          /* The exogenous attacker endpoint pool. THE WINDOWED ENGINE MUST
           * PASS THE SAME POINTER — an asymmetry here is an immediate
           * test_arch_equivalence divergence, because the two engines would
           * resolve different sessions for the same case. */
          .attackers = &infra.attackers,
          .fraudSeed = legit_.runScope().seed ^ 0x9E3779B97F4A7C15ULL,
      },
      /* The timeline carrier gives each ring plan its alive horizon: ring
       * scheduling never recruits the dead. */
      fraud_.ringView(people.roster.topology, people.personas.timelines),
      FraudEmission::accountView(holdings.accounts.registry,
                                 holdings.accounts.ownership),
      fraud_.resolvedBehavior(),
  };
}

} // namespace PhantomLedger::pipeline::stages::transfers
