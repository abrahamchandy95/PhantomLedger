#include "phantomledger/transfers/legit/ledger/builder.hpp"

#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <stdexcept>

namespace PhantomLedger::transfers::legit::ledger {

blueprints::TransfersPayload LegitTransferBuilder::build() const {
  if (request == nullptr) {
    throw std::invalid_argument(
        "LegitTransferBuilder.build() requires a non-null request");
  }

  if (request->network.accounts == nullptr ||
      request->network.accounts->records.empty()) {
    return blueprints::TransfersPayload{};
  }

  auto plan = blueprints::buildLegitPlan(request->timeline, request->network,
                                         request->macro, request->overrides);

  auto initialBook = buildBalanceBook(request->timeline, request->network,
                                      request->specs, request->ccState, plan);

  TxnStreams streams;
  ScreenBook screen{initialBook.get()};

  if (request->timeline.rng == nullptr) {
    throw std::invalid_argument(
        "LegitTransferBuilder.build() requires a non-null timeline.rng");
  }
  const transactions::Factory txf(*request->timeline.rng,
                                  request->overrides.infra);

  passes::GovernmentCounterparties govCps{};

  passes::addIncome(*request, plan, txf, streams, govCps);

  if (request->network.ownership != nullptr &&
      request->network.accounts != nullptr) {
    passes::addRoutines(*request, plan, *request->network.ownership,
                        *request->network.accounts, txf, streams, screen);
  }

  if (famGraphCfg != nullptr && familyTransfers != nullptr) {
    passes::addFamily(*request, plan, txf, streams, *famGraphCfg,
                      *familyTransfers);
  }

  passes::addCredit(*request, plan, txf, streams);

  // 6. Payload packing
  blueprints::TransfersPayload payload;
  payload.candidateTxns = streams.takeCandidates();
  payload.hubAccounts = std::move(plan.counterparties.hubAccounts);
  payload.billerAccounts = std::move(plan.counterparties.billerAccounts);
  payload.employers = std::move(plan.counterparties.employers);
  payload.initialBook = std::move(initialBook);
  payload.replaySortedTxns = streams.takeReplayReady();

  return payload;
}

} // namespace PhantomLedger::transfers::legit::ledger
