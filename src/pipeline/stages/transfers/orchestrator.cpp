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

TransferStage &TransferStage::infra(const pipeline::Infra &value) noexcept {
  infra_ = &value;
  return *this;
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

} // namespace

legit::ledger::LegitTransferResult
TransferStage::buildLegit(::PhantomLedger::random::Rng &rng,
                          const pipeline::People &people,
                          const pipeline::Holdings &holdings,
                          const pipeline::Counterparties &cps) const {
  legit_.validate();
  const auto &infra = requireInfra(infra_);
  auto builder = legit_.builder(rng, people, holdings, cps);
  builder.router(infra.router);
  auto result = builder.build();
  if (!result.openingBook.hasInitialBook()) {
    throw std::runtime_error(
        "transfers::TransferStage::buildLegit: legit builder produced no "
        "initial book");
  }
  ::PhantomLedger::pipeline::validateTransactionAccounts(
      holdings.accounts.lookup, result.txns.candidateTxns);
  return result;
}

std::vector<Transaction>
TransferStage::mergeProducts(::PhantomLedger::random::Rng &rng,
                             const pipeline::Holdings &holdings,
                             legit::ledger::LegitTxnStreams streams) const {
  const auto &infra = requireInfra(infra_);
  const auto scope = legit_.runScope();
  const auto primaryAccountsByPerson = primaryAccounts(holdings);

  ::PhantomLedger::transactions::Factory productTxf{rng, &infra.router,
                                                    &infra.ringInfra};
  ProductTxnEmitter productEmitter{scope.window, scope.seed, rng, productTxf};

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
      },
      fraud_.ringView(people.roster.topology),
      FraudEmission::accountView(holdings.accounts.registry,
                                 holdings.accounts.ownership),
      fraud_.resolvedBehavior(),
  };
}

} // namespace PhantomLedger::pipeline::stages::transfers
