#include "phantomledger/spending/simulator/run_planner.hpp"

#include "phantomledger/spending/simulator/payday_index.hpp"
#include "phantomledger/spending/spenders/prepare.hpp"
#include "phantomledger/spending/spenders/targets.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {
namespace {

void resolvePersonPrimaryIdx(const market::Market &market,
                             const clearing::Ledger &ledger,
                             std::vector<clearing::Ledger::Index> &out) {
  const auto &pop = market.population();
  const auto count = pop.count();
  out.assign(count, clearing::Ledger::invalid);

  for (std::uint32_t i = 0; i < count; ++i) {
    const auto person = static_cast<entity::PersonId>(i + 1);
    const auto key = pop.primary(person);
    if (entity::valid(key)) {
      out[i] = ledger.findAccount(key);
    }
  }
}

void resolveMerchantCounterpartyIdx(const market::Market &market,
                                    const clearing::Ledger &ledger,
                                    std::vector<clearing::Ledger::Index> &out) {
  const auto *catalog = market.commerce().catalog();
  if (catalog == nullptr) {
    out.clear();
    return;
  }

  out.assign(catalog->records.size(), clearing::Ledger::invalid);

  for (std::size_t i = 0; i < catalog->records.size(); ++i) {
    out[i] = ledger.findAccount(catalog->records[i].counterpartyId);
  }
}

} // namespace

RunPlanner::RunPlanner(TransactionLoad load, routing::ChannelWeights channels,
                       routing::PaymentRoutingRules paymentRules)
    : load_(load), channels_(channels), paymentRules_(paymentRules) {}

RunPlan RunPlanner::build(const market::Market &market,
                          const obligations::Snapshot &obligations,
                          const clearing::Ledger *ledger,
                          std::span<const double> sensitivities) const {
  RunPlan plan{};

  plan.population.spenders =
      spenders::prepareSpenders(market, obligations, ledger);
  plan.population.sensitivities = sensitivities;

  const std::uint32_t activeSpenders = plan.population.activeCount();

  plan.budget.targetTotalTxns = spenders::totalTargetTxns(
      load_.txnsPerMonth, activeSpenders, market.bounds().days);

  plan.budget.totalPersonDays =
      static_cast<std::uint64_t>(activeSpenders) * market.bounds().days;

  if (load_.personDailyLimit > 0) {
    plan.budget.personLimit = load_.personDailyLimit;
  }

  plan.baseLedger.txns = obligations.baseTxns;
  plan.payday.index =
      PaydayIndex::build(market.population().paydays(), market.bounds().days);

  plan.routing.channelCdf = channels_.cdf();
  plan.routing.paymentRules = paymentRules_;

  if (ledger != nullptr) {
    resolvePersonPrimaryIdx(market, *ledger, plan.routing.personPrimaryIdx);
    resolveMerchantCounterpartyIdx(market, *ledger,
                                   plan.routing.merchantCounterpartyIdx);

    const entity::Key externalKey =
        entity::makeKey(entity::Role::merchant, entity::Bank::external, 1u);

    plan.routing.externalUnknownIdx = ledger->findAccount(externalKey);
  }

  return plan;
}

} // namespace PhantomLedger::spending::simulator
