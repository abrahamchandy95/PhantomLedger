#include "phantomledger/transfers/legit/ledger/builder.hpp"

#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <stdexcept>
#include <utility>

namespace PhantomLedger::transfers::legit::ledger {

namespace {

namespace relatives = ::PhantomLedger::transfers::legit::routines::relatives;

[[nodiscard]] relatives::FamilyLedgerSources
familySourcesFrom(const passes::FamilyPass &pass) noexcept {
  const auto accounts = pass.accounts();

  return relatives::FamilyLedgerSources{
      .accounts = accounts.registry,
      .ownership = accounts.ownership,
      .educationMerchants = pass.merchants(),
  };
}

} // namespace

LegitTransferBuilder::LegitTransferBuilder(random::Rng &rng,
                                           blueprints::LegitTimeframe timeframe,
                                           blueprints::AccountCensus census,
                                           OpeningBook openingBook) noexcept
    : rng_(&rng), timeframe_(timeframe), census_(census),
      openingBook_(std::move(openingBook)) {}

LegitTransferBuilder &LegitTransferBuilder::rng(random::Rng &value) noexcept {
  rng_ = &value;
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::timeframe(blueprints::LegitTimeframe value) noexcept {
  timeframe_ = value;
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::census(blueprints::AccountCensus value) noexcept {
  census_ = value;
  return *this;
}

LegitTransferBuilder &LegitTransferBuilder::counterparties(
    blueprints::CounterpartyPools value) noexcept {
  counterparties_ = value;
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::personas(blueprints::PersonaCatalog value) noexcept {
  personas_ = value;
  return *this;
}

LegitTransferBuilder &LegitTransferBuilder::hubSelection(
    blueprints::HubSelectionRules value) noexcept {
  hubSelection_ = value;
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::openingBook(OpeningBook value) noexcept {
  openingBook_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::income(passes::IncomePass value) noexcept {
  income_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::routines(passes::RoutinePass value) noexcept {
  routines_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::family(passes::FamilyPass value) noexcept {
  family_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::credit(passes::CreditLifecyclePass value) noexcept {
  credit_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::familyScenario(FamilyTransferScenario value) noexcept {
  familyScenario_ = value;
  return *this;
}

LegitTransferBuilder &LegitTransferBuilder::router(
    const ::PhantomLedger::infra::Router &value) noexcept {
  router_ = &value;
  return *this;
}

LegitTransferBuilder &LegitTransferBuilder::router(
    const ::PhantomLedger::infra::Router *value) noexcept {
  router_ = value;
  return *this;
}

const entity::account::Registry *
LegitTransferBuilder::accounts() const noexcept {
  return census_.accounts;
}

LegitTransferResult LegitTransferBuilder::build() const {
  if (rng_ == nullptr) {
    throw std::invalid_argument(
        "LegitTransferBuilder.build() requires a non-null rng");
  }

  const auto *accountRegistry = accounts();
  if (accountRegistry == nullptr || accountRegistry->records.empty()) {
    return LegitTransferResult{};
  }

  auto plan = blueprints::buildLegitBlueprint(timeframe_, census_);
  plan.addCounterparties(*rng_, census_, counterparties_, hubSelection_)
      .addPersonas(*rng_, timeframe_, personas_);

  auto initialBook = openingBook_.build(plan);

  TxnStreams streams;
  ScreenBook screen{initialBook.get()};

  const transactions::Factory txf(*rng_, router_);

  passes::addIncome(income_, plan, txf, streams);

  if (census_.ownership != nullptr && census_.accounts != nullptr) {
    auto routinePass = routines_;
    routinePass.accounts(passes::AccountAccess{
        .registry = census_.accounts,
        .ownership = census_.ownership,
    });
    routinePass.txf(txf);
    passes::addRoutines(routinePass, plan, streams, screen);
  }

  const random::RngFactory familyRngFactory{plan.seed()};
  streams.add(relatives::generateFamilyTxns(plan, familySourcesFrom(family_),
                                            familyRngFactory, txf,
                                            familyScenario_));

  passes::addCredit(credit_, plan, txf, streams);

  LegitTransferResult result;
  result.txns.candidateTxns = streams.takeCandidates();
  auto counterparties = std::move(plan).takeCounterparties();
  result.counterparties.hubAccounts = std::move(counterparties.hubAccounts);
  result.counterparties.billerAccounts =
      std::move(counterparties.billerAccounts);
  result.counterparties.employers = std::move(counterparties.employers);
  result.openingBook.initialBook = std::move(initialBook);
  result.txns.replaySortedTxns = streams.takeReplayReady();

  return result;
}

} // namespace PhantomLedger::transfers::legit::ledger
