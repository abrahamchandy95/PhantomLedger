#include "phantomledger/transfers/fraud/injector.hpp"

#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/dispatch.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace PhantomLedger::transfers::fraud {

namespace {

[[nodiscard]] std::int32_t ringBudget(std::int64_t remaining,
                                      std::int64_t ringsLeft) noexcept {
  const auto perRing = std::max<std::int64_t>(
      1, remaining / std::max<std::int64_t>(1, ringsLeft));
  return static_cast<std::int32_t>(std::min(perRing, remaining));
}

} // namespace

InjectionOutput inject(const InjectionInput &request) {
  const auto &scenario = request.scenario;
  const auto &runtime = request.runtime;
  const auto &params = request.params;

  // Defensive: a missing required input would otherwise produce a confusing
  // dereference deeper in the pipeline.
  if (runtime.rng == nullptr) {
    throw std::invalid_argument("Fraud injection requires a runtime.rng");
  }
  if (scenario.people == nullptr || scenario.topology == nullptr ||
      scenario.accounts == nullptr || scenario.ownership == nullptr) {
    throw std::invalid_argument(
        "Fraud injection requires people, topology, accounts and ownership");
  }
  if (scenario.fraudCfg == nullptr) {
    throw std::invalid_argument("Fraud injection requires a fraudCfg");
  }

  // Empty-rings shortcut: the scenario passes through unchanged.
  if (scenario.topology->rings.empty()) {
    return InjectionOutput{
        .txns = std::vector<transactions::Transaction>(
            scenario.baseTxns.begin(), scenario.baseTxns.end()),
        .injectedCount = 0,
    };
  }

  // Materialise the execution context. Factory rebinds onto the runtime's
  // RNG and reuses the router + ring infra so generated fraud rows carry
  // the same device/IP signatures the legit pipeline produces for ring
  // accounts.
  Execution execution{
      .txf = transactions::Factory(*runtime.rng, runtime.router,
                                   runtime.ringInfra),
      .rng = runtime.rng,
  };

  ActiveWindow window{
      .startDate = scenario.window.start,
      .days = scenario.window.days,
  };

  AccountPools pools{
      .allAccounts = {},
      .billerAccounts = request.counterparties.billerAccounts,
      .employers = request.counterparties.employers,
  };
  pools.allAccounts.reserve(scenario.accounts->records.size());
  for (const auto &record : scenario.accounts->records) {
    pools.allAccounts.push_back(record.id);
  }

  CamouflageContext camoCtx{
      .execution = execution,
      .window = window,
      .accounts = &pools,
      .rates = params.camouflage,
  };

  IllicitContext illicitCtx{
      .execution = execution,
      .window = window,
      .billerAccounts = std::span<const entity::Key>(
          pools.billerAccounts.data(), pools.billerAccounts.size()),
      .layeringRules = params.layering,
      .structuringRules = params.structuring,
  };

  // Build all ring plans up front. Plans are cheap (small vectors of Keys)
  // and we iterate them twice: once for camouflage, once for illicit.
  std::vector<Plan> ringPlans;
  ringPlans.reserve(scenario.topology->rings.size());
  for (const auto &ring : scenario.topology->rings) {
    ringPlans.push_back(buildPlan(ring, *scenario.topology, *scenario.accounts,
                                  *scenario.ownership));
  }

  // ---- Camouflage pass ------------------------------------------------
  std::vector<transactions::Transaction> camoTxns;
  for (const auto &plan : ringPlans) {
    auto produced = camouflage::generate(camoCtx, plan);
    camoTxns.insert(camoTxns.end(), std::make_move_iterator(produced.begin()),
                    std::make_move_iterator(produced.end()));
  }

  // ---- Budget calculation --------------------------------------------
  const auto targetIllicit = calculateIllicitBudget(
      static_cast<double>(scenario.fraudCfg->limits.targetIllicitP),
      static_cast<std::int64_t>(scenario.baseTxns.size() + camoTxns.size()));

  std::vector<transactions::Transaction> out;
  out.reserve(
      scenario.baseTxns.size() + camoTxns.size() +
      static_cast<std::size_t>(std::max<std::int64_t>(0, targetIllicit)));
  out.assign(scenario.baseTxns.begin(), scenario.baseTxns.end());

  if (targetIllicit <= 0) {
    out.insert(out.end(), std::make_move_iterator(camoTxns.begin()),
               std::make_move_iterator(camoTxns.end()));
    return InjectionOutput{
        .txns = std::move(out),
        .injectedCount = camoTxns.size(),
    };
  }

  // ---- Per-ring illicit pass -----------------------------------------
  std::vector<transactions::Transaction> illicitTxns;
  illicitTxns.reserve(static_cast<std::size_t>(targetIllicit));

  std::int64_t remainingBudget = targetIllicit;
  const auto totalRings = static_cast<std::int64_t>(ringPlans.size());

  for (std::int64_t ringIdx = 0; ringIdx < totalRings; ++ringIdx) {
    if (remainingBudget <= 0) {
      break;
    }

    const auto perRing = ringBudget(remainingBudget, totalRings - ringIdx);
    const auto typology = params.typology.choose(*runtime.rng);
    auto produced =
        typologies::generate(illicitCtx, ringPlans[ringIdx], typology, perRing);

    remainingBudget -= static_cast<std::int64_t>(produced.size());
    illicitTxns.insert(illicitTxns.end(),
                       std::make_move_iterator(produced.begin()),
                       std::make_move_iterator(produced.end()));
  }

  const auto camoCount = camoTxns.size();
  const auto illicitCount = illicitTxns.size();

  out.insert(out.end(), std::make_move_iterator(camoTxns.begin()),
             std::make_move_iterator(camoTxns.end()));
  out.insert(out.end(), std::make_move_iterator(illicitTxns.begin()),
             std::make_move_iterator(illicitTxns.end()));

  return InjectionOutput{
      .txns = std::move(out),
      .injectedCount = camoCount + illicitCount,
  };
}

} // namespace PhantomLedger::transfers::fraud
