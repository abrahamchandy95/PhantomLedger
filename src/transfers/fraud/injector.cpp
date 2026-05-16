#include "phantomledger/transfers/fraud/injector.hpp"

#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/playbook.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/dispatch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::transfers::fraud {

namespace {

[[nodiscard]] std::int32_t ringBudget(std::int64_t remaining,
                                      std::int64_t ringsLeft) noexcept {
  const auto perRing = std::max<std::int64_t>(
      1, remaining / std::max<std::int64_t>(1, ringsLeft));
  return static_cast<std::int32_t>(std::min(perRing, remaining));
}

[[nodiscard]] std::int32_t phaseBudget(std::int64_t ringRemaining,
                                       std::int32_t perRing, double fraction,
                                       bool isFinalPhase) noexcept {
  if (ringRemaining <= 0) {
    return 0;
  }
  if (isFinalPhase) {
    return static_cast<std::int32_t>(std::min<std::int64_t>(
        ringRemaining,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));
  }
  if (!(fraction > 0.0)) {
    return 0;
  }
  const auto raw = static_cast<std::int64_t>(
      std::ceil(static_cast<double>(perRing) * fraction));
  const auto capped = std::min<std::int64_t>(raw, ringRemaining);
  return static_cast<std::int32_t>(std::max<std::int64_t>(1, capped));
}

void requireInjectorPointers(const InjectorRingView &rings,
                             const InjectorAccountView &accounts) {
  if (rings.topology == nullptr || accounts.registry == nullptr ||
      accounts.ownership == nullptr) {
    throw std::invalid_argument(
        "Fraud injection requires topology, accounts and ownership");
  }
  if (rings.profile == nullptr) {
    throw std::invalid_argument(
        "Fraud injection requires a non-null InjectorRingView.profile");
  }
}

[[nodiscard]] Execution makeExecution(InjectorServices services) {
  return Execution{
      .txf = transactions::Factory(services.rng, services.router,
                                   services.ringInfra),
      .rng = &services.rng,
  };
}

[[nodiscard]] AccountPools
makeAccountPools(const entity::account::Registry &registry,
                 const InjectorLegitCounterparties &counterparties) {
  AccountPools pools{
      .allAccounts = {},
      .billerAccounts =
          std::vector<entity::Key>(counterparties.billerAccounts.begin(),
                                   counterparties.billerAccounts.end()),
      .employers = std::vector<entity::Key>(counterparties.employers.begin(),
                                            counterparties.employers.end()),
  };
  pools.allAccounts.reserve(registry.records.size());
  for (const auto &record : registry.records) {
    pools.allAccounts.push_back(record.id);
  }
  return pools;
}

[[nodiscard]] std::vector<Plan>
buildRingPlans(const entity::person::Topology &topology,
               const entity::account::Registry &registry,
               const entity::account::Ownership &ownership) {
  std::vector<Plan> plans;
  plans.reserve(topology.rings.size());
  for (const auto &ring : topology.rings) {
    plans.push_back(buildPlan(ring, topology, registry, ownership));
  }
  return plans;
}

[[nodiscard]] std::vector<transactions::Transaction>
generateCamouflage(CamouflageContext &ctx, std::span<const Plan> plans,
                   const camouflage::Rates &rates) {
  std::vector<transactions::Transaction> out;
  for (const auto &plan : plans) {
    auto produced = camouflage::generate(ctx, plan, rates);
    out.insert(out.end(), std::make_move_iterator(produced.begin()),
               std::make_move_iterator(produced.end()));
  }
  return out;
}

[[nodiscard]] std::vector<transactions::Transaction>
runRingPlaybook(const typologies::Dispatcher &dispatcher, const Plan &plan,
                std::int32_t perRing, const Playbook &playbook) {
  std::vector<transactions::Transaction> out;
  if (perRing <= 0) {
    return out;
  }

  std::int64_t ringRemaining = perRing;
  const auto phaseCount = playbook.phases.size();

  for (std::size_t phaseIdx = 0; phaseIdx < phaseCount; ++phaseIdx) {
    if (ringRemaining <= 0) {
      break;
    }
    const auto &phase = playbook.phases[phaseIdx];
    const bool isFinal = (phaseIdx + 1 == phaseCount);
    const auto budget =
        phaseBudget(ringRemaining, perRing, phase.budgetFraction, isFinal);
    if (budget <= 0) {
      continue;
    }
    auto produced = dispatcher.run(plan, phase.typology, budget);
    ringRemaining -= static_cast<std::int64_t>(produced.size());
    out.insert(out.end(), std::make_move_iterator(produced.begin()),
               std::make_move_iterator(produced.end()));
  }
  return out;
}

[[nodiscard]] std::vector<transactions::Transaction>
generateIllicit(IllicitContext &ctx, const Behavior &behavior,
                std::span<const Plan> plans, std::int64_t targetIllicit) {
  std::vector<transactions::Transaction> out;
  if (targetIllicit <= 0 || plans.empty()) {
    return out;
  }
  out.reserve(static_cast<std::size_t>(targetIllicit));

  std::int64_t remainingBudget = targetIllicit;
  const auto totalRings = static_cast<std::int64_t>(plans.size());
  const typologies::Dispatcher dispatcher{ctx, behavior};

  for (std::int64_t ringIdx = 0; ringIdx < totalRings; ++ringIdx) {
    if (remainingBudget <= 0) {
      break;
    }
    const auto perRing = ringBudget(remainingBudget, totalRings - ringIdx);
    const auto &playbook = behavior.playbooks.choose(*ctx.execution.rng);

    auto produced =
        runRingPlaybook(dispatcher, plans[ringIdx], perRing, playbook);
    remainingBudget -= static_cast<std::int64_t>(produced.size());
    out.insert(out.end(), std::make_move_iterator(produced.begin()),
               std::make_move_iterator(produced.end()));
  }
  return out;
}

[[nodiscard]] InjectionOutput
passthrough(std::span<const transactions::Transaction> baseTxns) {
  return InjectionOutput{
      .txns = std::vector<transactions::Transaction>(baseTxns.begin(),
                                                     baseTxns.end()),
      .injectedCount = 0,
  };
}

[[nodiscard]] InjectionOutput
assembleOutput(std::span<const transactions::Transaction> baseTxns,
               std::vector<transactions::Transaction> &&camoTxns,
               std::vector<transactions::Transaction> &&illicitTxns) {
  const auto camoCount = camoTxns.size();
  const auto illicitCount = illicitTxns.size();

  std::vector<transactions::Transaction> out;
  out.reserve(baseTxns.size() + camoCount + illicitCount);
  out.assign(baseTxns.begin(), baseTxns.end());
  out.insert(out.end(), std::make_move_iterator(camoTxns.begin()),
             std::make_move_iterator(camoTxns.end()));
  out.insert(out.end(), std::make_move_iterator(illicitTxns.begin()),
             std::make_move_iterator(illicitTxns.end()));

  return InjectionOutput{
      .txns = std::move(out),
      .injectedCount = camoCount + illicitCount,
  };
}

} // namespace

Injector::Injector(InjectorServices services, InjectorRingView rings,
                   InjectorAccountView accounts,
                   const Behavior &behavior) noexcept
    : services_(services), rings_(rings), accounts_(accounts),
      behavior_(behavior) {}

InjectionOutput
Injector::inject(time::Window window,
                 std::span<const transactions::Transaction> baseTxns) const {
  return inject(window, baseTxns, InjectorLegitCounterparties{});
}

InjectionOutput
Injector::inject(time::Window window,
                 std::span<const transactions::Transaction> baseTxns,
                 InjectorLegitCounterparties counterparties) const {
  requireInjectorPointers(rings_, accounts_);

  if (rings_.topology->rings.empty()) {
    return passthrough(baseTxns);
  }

  Execution execution = makeExecution(services_);
  AccountPools pools = makeAccountPools(*accounts_.registry, counterparties);

  CamouflageContext camouflageCtx{
      .execution = execution,
      .window = window,
      .accounts = &pools,
  };

  IllicitContext illicitCtx{
      .execution = execution,
      .window = window,
      .billerAccounts = std::span<const entity::Key>(
          pools.billerAccounts.data(), pools.billerAccounts.size()),
  };

  const auto ringPlans = buildRingPlans(*rings_.topology, *accounts_.registry,
                                        *accounts_.ownership);

  auto camoTxns = generateCamouflage(
      camouflageCtx, std::span<const Plan>(ringPlans), behavior_.camouflage);

  const auto targetIllicit = calculateIllicitBudget(
      static_cast<double>(rings_.profile->limits.targetIllicitP),
      static_cast<std::int64_t>(baseTxns.size() + camoTxns.size()));

  auto illicitTxns = generateIllicit(
      illicitCtx, behavior_, std::span<const Plan>(ringPlans), targetIllicit);

  return assembleOutput(baseTxns, std::move(camoTxns), std::move(illicitTxns));
}

} // namespace PhantomLedger::transfers::fraud
