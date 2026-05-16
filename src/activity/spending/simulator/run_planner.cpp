#include "phantomledger/activity/spending/simulator/run_planner.hpp"

#include "phantomledger/activity/spending/simulator/payday_index.hpp"
#include "phantomledger/activity/spending/spenders/prepare.hpp"
#include "phantomledger/activity/spending/spenders/targets.hpp"

#include <cstdint>

namespace PhantomLedger::activity::spending::simulator {
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

[[nodiscard]] PreparedRun::Population preparePopulation(
    const market::Market &market, const obligations::Snapshot &obligations,
    const clearing::Ledger *ledger, std::span<const double> sensitivities) {
  PreparedRun::Population population;
  population.spenders = spenders::prepareSpenders(market, obligations, ledger);
  population.sensitivities = sensitivities;
  return population;
}

[[nodiscard]] PreparedRun::Budget
buildBudget(const RunPlanner::TransactionLoad &load,
            std::uint32_t activeSpenders, std::uint32_t days) {
  PreparedRun::Budget budget;
  budget.targetTotalTxns =
      spenders::totalTargetTxns(load.txnsPerMonth, activeSpenders, days);
  budget.totalPersonDays = static_cast<std::uint64_t>(activeSpenders) * days;

  if (load.personDailyLimit > 0) {
    budget.personLimit = load.personDailyLimit;
  }

  return budget;
}

[[nodiscard]] PreparedRun::LedgerReplay
ledgerReplayFrom(const obligations::Snapshot &obligations) noexcept {
  return PreparedRun::LedgerReplay{.txns = obligations.baseTxns};
}

[[nodiscard]] PreparedRun::Paydays
preparePaydays(const market::Market &market) {
  return PreparedRun::Paydays{
      .index = PaydayIndex::build(market.population().paydays(),
                                  market.bounds().days),
  };
}

[[nodiscard]] PreparedRun::Routing
prepareRouting(const market::Market &market, const clearing::Ledger *ledger,
               const routing::ChannelWeights &channels,
               const routing::PaymentRoutingRules &paymentRules) {
  PreparedRun::Routing routing;
  routing.channelCdf = channels.cdf();
  routing.paymentRules = paymentRules;

  if (ledger != nullptr) {
    resolvePersonPrimaryIdx(market, *ledger, routing.personPrimaryIdx);
    resolveMerchantCounterpartyIdx(market, *ledger,
                                   routing.merchantCounterpartyIdx);

    const entity::Key externalKey =
        entity::makeKey(entity::Role::merchant, entity::Bank::external, 1u);

    routing.externalUnknownIdx = ledger->findAccount(externalKey);
  }

  return routing;
}

} // namespace

RunPlanner::RunPlanner(TransactionLoad transactionLoad,
                       routing::ChannelWeights channels,
                       routing::PaymentRoutingRules paymentRules)
    : transactionLoad_(transactionLoad), channels_(channels),
      paymentRules_(paymentRules) {}

PreparedRun RunPlanner::build(const market::Market &market,
                              const obligations::Snapshot &obligations,
                              const clearing::Ledger *ledger,
                              std::span<const double> sensitivities) const {
  PreparedRun run;

  run.population() =
      preparePopulation(market, obligations, ledger, sensitivities);
  run.budget() = buildBudget(transactionLoad_, run.population().activeCount(),
                             market.bounds().days);
  run.ledgerReplay() = ledgerReplayFrom(obligations);
  run.paydays() = preparePaydays(market);
  run.routing() = prepareRouting(market, ledger, channels_, paymentRules_);

  return run;
}

} // namespace PhantomLedger::activity::spending::simulator
