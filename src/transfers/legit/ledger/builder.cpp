#include "phantomledger/transfers/legit/ledger/builder.hpp"

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

} // namespace

LegitTransferBuilder::LegitTransferBuilder(
    random::Rng &rng, blueprints::LegitTimeframe timeframe,
    blueprints::AccountCensus census, BalanceBookRequest openingBook) noexcept
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
LegitTransferBuilder::openingBook(BalanceBookRequest value) noexcept {
  openingBook_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::income(passes::IncomePassRequest value) noexcept {
  income_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::routines(passes::RoutinePassRequest value) noexcept {
  routines_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::family(passes::FamilyPassRequest value) noexcept {
  family_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::credit(passes::CreditPassRequest value) noexcept {
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

  auto plan = blueprints::buildLegitPlan(
      *rng_, timeframe_, census_, counterparties_, personas_, hubSelection_);

  auto initialBook = buildBalanceBook(openingBook_, plan);

  TxnStreams streams;
  ScreenBook screen{initialBook.get()};

  const transactions::Factory txf(*rng_, router_);

  passes::GovernmentCounterparties govCps{};

  passes::addIncome(income_, plan, txf, streams, govCps);

  if (census_.ownership != nullptr && census_.accounts != nullptr) {
    passes::addRoutines(routines_, plan, *census_.ownership, *census_.accounts,
                        txf, streams, screen);
  }

  if (familyPrograms_.transfers != nullptr) {
    passes::addFamily(
        family_, plan, txf, streams, familyHouseholdsFrom(familyPrograms_),
        familyDependentsFrom(familyPrograms_),
        retireeSupportFrom(familyPrograms_), *familyPrograms_.transfers);
  }

  passes::addCredit(credit_, plan, txf, streams);

  // Payload packing.
  TransfersPayload payload;
  payload.candidateTxns = streams.takeCandidates();
  payload.hubAccounts = std::move(plan.counterparties.hubAccounts);
  payload.billerAccounts = std::move(plan.counterparties.billerAccounts);
  payload.employers = std::move(plan.counterparties.employers);
  payload.initialBook = std::move(initialBook);
  payload.replaySortedTxns = streams.takeReplayReady();

  return payload;
}

} // namespace PhantomLedger::transfers::legit::ledger
