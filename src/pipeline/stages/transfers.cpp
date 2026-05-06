#include "phantomledger/pipeline/stages/transfers.hpp"

#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

namespace validate = ::PhantomLedger::primitives::validate;

using Transaction = ::PhantomLedger::transactions::Transaction;

constexpr double kFraudHeadroomFraction = 0.05;

[[nodiscard]] std::size_t fraudMergedCapacity(std::size_t legitSize) noexcept {
  return legitSize + static_cast<std::size_t>(static_cast<double>(legitSize) *
                                              kFraudHeadroomFraction);
}

} // namespace

TransferStage &TransferStage::runScope(RunScope value) noexcept {
  legit_.runScope(value);
  return *this;
}

TransferStage &TransferStage::incomePrograms(const IncomePrograms &value) {
  legit_.incomePrograms(value);
  return *this;
}

TransferStage &TransferStage::openingBalances(OpeningBalances value) noexcept {
  legit_.openingBalances(value);
  return *this;
}

TransferStage &TransferStage::cardLifecycle(CardLifecycle value) noexcept {
  legit_.cardLifecycle(value);
  return *this;
}

TransferStage &
TransferStage::familyTransfers(FamilyTransferScenario value) noexcept {
  legit_.familyTransfers(value);
  return *this;
}

TransferStage &
TransferStage::insurancePrograms(InsurancePrograms value) noexcept {
  products_.insurancePrograms(value);
  return *this;
}

TransferStage &TransferStage::replayOrdering(ReplayOrdering value) noexcept {
  ledger_.ordering(value);
  return *this;
}

TransferStage &TransferStage::fraudInjection(FraudInjection value) noexcept {
  fraud_.programs(value);
  return *this;
}

TransferStage &TransferStage::hubSelection(HubSelection value) noexcept {
  legit_.hubSelection(value);
  return *this;
}

TransferStage &
TransferStage::window(::PhantomLedger::time::Window value) noexcept {
  legit_.window(value);
  return *this;
}

TransferStage &TransferStage::seed(std::uint64_t value) noexcept {
  legit_.seed(value);
  return *this;
}

TransferStage &TransferStage::recurringIncome(
    const ::PhantomLedger::inflows::RecurringIncomeRules &value) {
  legit_.recurringIncome(value);
  return *this;
}

TransferStage &TransferStage::employmentRules(
    const ::PhantomLedger::recurring::EmploymentRules &value) {
  legit_.employmentRules(value);
  return *this;
}

TransferStage &
TransferStage::leaseRules(const ::PhantomLedger::recurring::LeaseRules &value) {
  legit_.leaseRules(value);
  return *this;
}

TransferStage &TransferStage::salaryPaidFraction(double value) noexcept {
  legit_.salaryPaidFraction(value);
  return *this;
}

TransferStage &TransferStage::rentPaidFraction(double value) noexcept {
  legit_.rentPaidFraction(value);
  return *this;
}

TransferStage &TransferStage::openingBalanceRules(
    const ::PhantomLedger::clearing::BalanceRules *value) noexcept {
  legit_.openingBalanceRules(value);
  return *this;
}

TransferStage &TransferStage::creditLifecycle(
    const ::PhantomLedger::transfers::credit_cards::LifecycleRules
        *value) noexcept {
  legit_.creditLifecycle(value);
  return *this;
}

TransferStage &TransferStage::family(FamilyTransferScenario value) noexcept {
  legit_.family(value);
  return *this;
}

TransferStage &TransferStage::retirementBenefits(
    const ::PhantomLedger::transfers::government::RetirementTerms &value) {
  legit_.retirementBenefits(value);
  return *this;
}

TransferStage &TransferStage::disabilityBenefits(
    const ::PhantomLedger::transfers::government::DisabilityTerms &value) {
  legit_.disabilityBenefits(value);
  return *this;
}

TransferStage &TransferStage::insuranceClaims(
    ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept {
  products_.insuranceClaims(value);
  return *this;
}

TransferStage &TransferStage::replayRules(
    ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
        value) noexcept {
  ledger_.rules(value);
  return *this;
}

TransferStage &TransferStage::fraudProfile(
    const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept {
  fraud_.profile(value);
  return *this;
}

TransferStage &TransferStage::fraudRules(
    ::PhantomLedger::transfers::fraud::Injector::Rules value) noexcept {
  fraud_.rules(value);
  return *this;
}

TransferStage &TransferStage::hubFraction(double value) noexcept {
  legit_.hubSelection(HubSelection{.fraction = value});
  return *this;
}

const TransferStage::RunScope &TransferStage::runScope() const noexcept {
  return legit_.runScope();
}

::PhantomLedger::pipeline::Transfers
TransferStage::build(::PhantomLedger::random::Rng &rng,
                     const ::PhantomLedger::pipeline::Entities &entities,
                     const ::PhantomLedger::pipeline::Infra &infra) const {
  validate::require(legit_.incomePrograms().recurring);
  legit_.validate();

  auto builder = legit_.builder(rng, entities, infra);
  auto legitPayload = builder.build();

  if (legitPayload.initialBook == nullptr) {
    throw std::runtime_error(
        "transfers::TransferStage::build: legit builder produced no initial "
        "book");
  }

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities.accounts.lookup, legitPayload.candidateTxns);

  const auto primaryAccountsByPerson = primaryAccounts(entities);
  auto replaySortedStream =
      products_.merge(runScope().window, runScope().seed, rng, entities, infra,
                      primaryAccountsByPerson, legitPayload);

  auto preReplay = ledger_.preFraud(*legitPayload.initialBook, rng,
                                    std::move(replaySortedStream));

  const std::span<const Transaction> draftView{preReplay.draftTxns.data(),
                                               preReplay.draftTxns.size()};
  auto fraudOut =
      fraud_.inject(rng, infra.router, infra.ringInfra, runScope().window,
                    entities.people.topology, entities.accounts.registry,
                    entities.accounts.ownership, draftView, legitPayload);

  auto mergedTxns = std::move(fraudOut.txns);
  mergedTxns.reserve(fraudMergedCapacity(mergedTxns.size()));

  auto postReplay =
      ledger_.postFraud(rng, *legitPayload.initialBook, std::move(mergedTxns));

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities.accounts.lookup, postReplay.finalTxns);

  ::PhantomLedger::pipeline::Transfers out{};
  out.legit = std::move(legitPayload);
  out.fraud.injectedCount = fraudOut.injectedCount;
  out.draftTxns = std::move(preReplay.draftTxns);
  out.finalTxns = std::move(postReplay.finalTxns);
  out.finalBook = std::move(postReplay.finalBook);
  out.dropCounts = std::move(preReplay.dropCounts);
  out.dropCountsByChannel = std::move(preReplay.dropCountsByChannel);

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
