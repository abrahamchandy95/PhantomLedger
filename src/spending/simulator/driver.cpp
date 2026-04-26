#include "phantomledger/spending/simulator/driver.hpp"

#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/simulator/day.hpp"
#include "phantomledger/spending/spenders/prepare.hpp"
#include "phantomledger/spending/spenders/targets.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>

namespace PhantomLedger::spending::simulator {

namespace {

constexpr double kTxnReserveSlack = 1.05;

void sortChronological(std::vector<transactions::Transaction> &txns) {
  // Sort by (timestamp, source, target, amount) — the canonical key.
  std::sort(
      txns.begin(), txns.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
}

} // namespace

Simulator::Simulator(const market::Market &market, Engine &engine,
                     const obligations::Snapshot &obligations,
                     const dynamics::Config &dynamicsCfg,
                     const config::BurstBehavior &burst,
                     const config::ExplorationHabits &exploration,
                     const config::LiquidityConstraints &liquidity,
                     const config::MerchantPickRules &picking,
                     const math::seasonal::Config &seasonal,
                     double txnsPerMonth, double channelMerchantP,
                     double channelBillsP, double channelP2pP,
                     double unknownOutflowP, double baseExploreP,
                     double dayShockShape, double preferBillersP,
                     std::uint32_t personDailyLimit)
    : market_(market), engine_(engine), obligations_(obligations),
      dynamicsCfg_(dynamicsCfg), burst_(burst), exploration_(exploration),
      liquidity_(liquidity), picking_(picking), seasonal_(seasonal),
      txnsPerMonth_(txnsPerMonth), channelMerchantP_(channelMerchantP),
      channelBillsP_(channelBillsP), channelP2pP_(channelP2pP),
      unknownOutflowP_(unknownOutflowP), baseExploreP_(baseExploreP),
      dayShockShape_(dayShockShape), preferBillersP_(preferBillersP),
      personDailyLimit_(personDailyLimit),
      cohort_(market.population().count()) {
  const auto n = market.population().count();

  // Per-person paycheck sensitivity: drawn here from each persona.
  // `entity::behavior::Persona` nests this under `payday.sensitivity`;
  // the previous flat `paycheckSensitivity` accessor doesn't exist.
  sensitivities_.resize(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    const auto person = static_cast<entity::PersonId>(i + 1);
    sensitivities_[i] = market.population().object(person).payday.sensitivity;
  }

  paydayPersonIndicesScratch_.reserve(n);
  momentumMultBuffer_.resize(n);
  dormancyMultBuffer_.resize(n);
  paycheckMultBuffer_.resize(n);
  dailyMultBuffer_.resize(n);
}

RunPlan Simulator::buildPlan() const {
  RunPlan plan{};

  plan.preparedSpenders = spenders::prepareSpenders(market_, obligations_);
  const std::uint32_t activeSpenders =
      static_cast<std::uint32_t>(plan.preparedSpenders.size());

  plan.targetTotalTxns = spenders::totalTargetTxns(
      txnsPerMonth_, activeSpenders, market_.bounds().days);

  plan.totalPersonDays =
      static_cast<std::uint64_t>(activeSpenders) * market_.bounds().days;

  plan.baseTxns = obligations_.baseTxns;
  plan.sensitivities = std::span<const double>(sensitivities_);

  if (personDailyLimit_ > 0) {
    plan.personLimit = personDailyLimit_;
  }

  plan.channelCdf = routing::buildChannelCdf(channelMerchantP_, channelBillsP_,
                                             channelP2pP_, unknownOutflowP_);

  plan.routePolicy = routing::Policy{
      .preferBillersP = preferBillersP_,
      .maxRetries = picking_.maxRetries,
  };

  plan.baseExploreP = baseExploreP_;
  plan.dayShockShape = dayShockShape_;

  return plan;
}

std::vector<transactions::Transaction> Simulator::run() {
  if (market_.bounds().days == 0) {
    return {};
  }

  RunPlan plan = buildPlan();

  // Reserve up front: target * 1.05 covers Poisson overshoot.
  const std::size_t reserveCapacity =
      static_cast<std::size_t>(plan.targetTotalTxns * kTxnReserveSlack);

  RunState state(market_.population().count(), reserveCapacity);

  for (std::uint32_t dayIndex = 0; dayIndex < market_.bounds().days;
       ++dayIndex) {
    runDay(market_, engine_, plan, state, cohort_, dynamicsCfg_, burst_,
           exploration_, liquidity_, seasonal_, dayIndex);
  }

  auto txns = std::move(state.txns());
  sortChronological(txns);
  return txns;
}

} // namespace PhantomLedger::spending::simulator
