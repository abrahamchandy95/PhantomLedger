#include "phantomledger/transfers/legit/ledger/builder.hpp"

#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <stdexcept>

namespace PhantomLedger::transfers::legit::ledger {

namespace {

[[nodiscard]] const entity::account::Registry *
accountsFrom(const LegitTransferRequest &request) noexcept {
  return request.plan.census.accounts;
}

[[nodiscard]] const relationships::family::Households &
familyHouseholdsFrom(const LegitTransferRequest &request) noexcept {
  return request.familyHouseholds != nullptr
             ? *request.familyHouseholds
             : relationships::family::kDefaultHouseholds;
}

[[nodiscard]] const relationships::family::Dependents &
familyDependentsFrom(const LegitTransferRequest &request) noexcept {
  return request.familyDependents != nullptr
             ? *request.familyDependents
             : relationships::family::kDefaultDependents;
}

[[nodiscard]] const relationships::family::RetireeSupport &
retireeSupportFrom(const LegitTransferRequest &request) noexcept {
  return request.retireeSupport != nullptr
             ? *request.retireeSupport
             : relationships::family::kDefaultRetireeSupport;
}

} // namespace

TransfersPayload LegitTransferBuilder::build() const {
  if (request == nullptr) {
    throw std::invalid_argument(
        "LegitTransferBuilder.build() requires a non-null request");
  }

  const auto *accounts = accountsFrom(*request);
  if (accounts == nullptr || accounts->records.empty()) {
    return TransfersPayload{};
  }

  auto plan = blueprints::buildLegitPlan(request->plan);

  auto initialBook = buildBalanceBook(request->balanceBook, plan);

  TxnStreams streams;
  ScreenBook screen{initialBook.get()};

  if (request->plan.rng == nullptr) {
    throw std::invalid_argument(
        "LegitTransferBuilder.build() requires a non-null rng");
  }
  const transactions::Factory txf(*request->plan.rng, request->router);

  passes::GovernmentCounterparties govCps{};

  passes::addIncome(request->income, plan, txf, streams, govCps);

  if (request->plan.census.ownership != nullptr &&
      request->plan.census.accounts != nullptr) {
    passes::addRoutines(request->routines, plan,
                        *request->plan.census.ownership,
                        *request->plan.census.accounts, txf, streams, screen);
  }

  if (request->familyTransfers != nullptr) {
    passes::addFamily(request->family, plan, txf, streams,
                      familyHouseholdsFrom(*request),
                      familyDependentsFrom(*request),
                      retireeSupportFrom(*request), *request->familyTransfers);
  }

  passes::addCredit(request->credit, plan, txf, streams);

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
