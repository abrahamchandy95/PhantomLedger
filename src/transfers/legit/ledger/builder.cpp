#include "phantomledger/transfers/legit/ledger/builder.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

namespace {

namespace relatives = ::PhantomLedger::transfers::legit::routines::relatives;

[[nodiscard]] const relationships::family::Households &familyHouseholdsFrom(
    const LegitTransferBuilder::FamilyPrograms &family) noexcept {
  return family.households != nullptr
             ? *family.households
             : relationships::family::kDefaultHouseholds;
}

[[nodiscard]] const relationships::family::Dependents &familyDependentsFrom(
    const LegitTransferBuilder::FamilyPrograms &family) noexcept {
  return family.dependents != nullptr
             ? *family.dependents
             : relationships::family::kDefaultDependents;
}

[[nodiscard]] const relationships::family::RetireeSupport &retireeSupportFrom(
    const LegitTransferBuilder::FamilyPrograms &family) noexcept {
  return family.retireeSupport != nullptr
             ? *family.retireeSupport
             : relationships::family::kDefaultRetireeSupport;
}

[[nodiscard]] relatives::FamilyLedgerSources
familySourcesFrom(const passes::FamilyPass &pass) noexcept {
  const auto accounts = pass.accounts();

  return relatives::FamilyLedgerSources{
      .accounts = accounts.registry,
      .ownership = accounts.ownership,
      .educationMerchants = pass.merchants(),
  };
}

[[nodiscard]] std::vector<transactions::Transaction>
familyTxnsFrom(const passes::FamilyPass &pass,
               const LegitTransferBuilder::FamilyPrograms &programs,
               const blueprints::LegitBlueprint &plan,
               const transactions::Factory &txf) {
  if (programs.transfers == nullptr) {
    return {};
  }

  const auto familySources = familySourcesFrom(pass);
  if (!relatives::canRun(familySources)) {
    return {};
  }

  const auto personas = relatives::personasView(plan);
  if (personas.empty() || relatives::personCount(plan) == 0) {
    return {};
  }

  const auto graph = relatives::buildFamilyGraph(
      plan, familyHouseholdsFrom(programs), familyDependentsFrom(programs),
      retireeSupportFrom(programs));

  const auto multipliers = relatives::amountMultipliers(plan);
  const random::RngFactory rngFactory{plan.seed()};

  const auto run = relatives::makeTransferRun(
      plan, graph, std::span<const double>{multipliers}, familySources,
      rngFactory, txf, programs.transfers->routing);

  return relatives::generateFamilyTxns(run, *programs.transfers);
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
LegitTransferBuilder::familyPrograms(FamilyPrograms value) noexcept {
  familyPrograms_ = value;
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

TransfersPayload LegitTransferBuilder::build() const {
  if (rng_ == nullptr) {
    throw std::invalid_argument(
        "LegitTransferBuilder.build() requires a non-null rng");
  }

  const auto *accountRegistry = accounts();
  if (accountRegistry == nullptr || accountRegistry->records.empty()) {
    return TransfersPayload{};
  }

  auto plan = blueprints::buildLegitBlueprint(
      *rng_, timeframe_, census_, counterparties_, personas_, hubSelection_);

  auto initialBook = openingBook_.build(plan);

  TxnStreams streams;
  ScreenBook screen{initialBook.get()};

  const transactions::Factory txf(*rng_, router_);

  passes::GovernmentCounterparties govCps{};

  passes::addIncome(income_, plan, txf, streams, govCps);

  if (census_.ownership != nullptr && census_.accounts != nullptr) {
    passes::addRoutines(routines_, plan, *census_.ownership, *census_.accounts,
                        txf, streams, screen);
  }

  streams.add(familyTxnsFrom(family_, familyPrograms_, plan, txf));

  passes::addCredit(credit_, plan, txf, streams);

  TransfersPayload payload;
  payload.candidateTxns = streams.takeCandidates();
  auto counterparties = std::move(plan).takeCounterparties();
  payload.hubAccounts = std::move(counterparties.hubAccounts);
  payload.billerAccounts = std::move(counterparties.billerAccounts);
  payload.employers = std::move(counterparties.employers);
  payload.initialBook = std::move(initialBook);
  payload.replaySortedTxns = streams.takeReplayReady();

  return payload;
}

} // namespace PhantomLedger::transfers::legit::ledger
