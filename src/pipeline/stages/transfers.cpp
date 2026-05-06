#include "phantomledger/pipeline/stages/transfers.hpp"

#include "phantomledger/pipeline/invariants.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

using Transaction = ::PhantomLedger::transactions::Transaction;

constexpr double kFraudHeadroomFraction = 0.05;

[[nodiscard]] std::size_t fraudMergedCapacity(std::size_t legitSize) noexcept {
  return legitSize + static_cast<std::size_t>(static_cast<double>(legitSize) *
                                              kFraudHeadroomFraction);
}

} // namespace

LegitAssembly &TransferStage::legit() noexcept { return legit_; }

const LegitAssembly &TransferStage::legit() const noexcept { return legit_; }

ProductReplay &TransferStage::products() noexcept { return products_; }

const ProductReplay &TransferStage::products() const noexcept {
  return products_;
}

LedgerReplay &TransferStage::ledger() noexcept { return ledger_; }

const LedgerReplay &TransferStage::ledger() const noexcept { return ledger_; }

FraudEmission &TransferStage::fraud() noexcept { return fraud_; }

const FraudEmission &TransferStage::fraud() const noexcept { return fraud_; }

::PhantomLedger::pipeline::Transfers
TransferStage::build(::PhantomLedger::random::Rng &rng,
                     const ::PhantomLedger::pipeline::Entities &entities,
                     const ::PhantomLedger::pipeline::Infra &infra) const {
  legit_.validate();

  auto builder = legit_.builder(rng, entities, infra);
  auto legitPayload = builder.build();

  if (!legitPayload.openingBook.hasInitialBook()) {
    throw std::runtime_error(
        "transfers::TransferStage::build: legit builder produced no initial "
        "book");
  }

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities.accounts.lookup, legitPayload.txns.candidateTxns);

  const auto scope = legit_.runScope();
  const auto primaryAccountsByPerson = primaryAccounts(entities);

  auto replaySortedStream =
      products_.merge(scope.window, scope.seed, rng, entities, infra,
                      primaryAccountsByPerson, legitPayload.txns);

  auto candidateReplay = ledger_.preFraud(*legitPayload.openingBook.initialBook,
                                          rng, std::move(replaySortedStream));

  const std::span<const Transaction> candidateView{candidateReplay.txns.data(),
                                                   candidateReplay.txns.size()};

  auto fraudInjector = fraud_.makeInjector(
      ::PhantomLedger::transfers::fraud::Injector::Services{
          .rng = rng,
          .router = &infra.router,
          .ringInfra = &infra.ringInfra,
      },
      fraud_.ringView(entities.people.topology),
      FraudEmission::accountView(entities.accounts.registry,
                                 entities.accounts.ownership));

  auto fraudOut = fraudInjector.inject(
      scope.window, candidateView,
      FraudEmission::legitCounterparties(legitPayload.counterparties));

  auto mergedTxns = std::move(fraudOut.txns);
  mergedTxns.reserve(fraudMergedCapacity(mergedTxns.size()));

  auto postedReplay = ledger_.postFraud(
      rng, *legitPayload.openingBook.initialBook, std::move(mergedTxns));

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities.accounts.lookup, postedReplay.txns);

  ::PhantomLedger::pipeline::Transfers out{};
  out.legit = std::move(legitPayload);
  out.fraud.injectedCount = fraudOut.injectedCount;
  out.ledger.candidate.txns = std::move(candidateReplay.txns);
  out.ledger.candidate.drops = std::move(candidateReplay.drops);
  out.ledger.posted.txns = std::move(postedReplay.txns);
  out.ledger.posted.book = std::move(postedReplay.book);

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
