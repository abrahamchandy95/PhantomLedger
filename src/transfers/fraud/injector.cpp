#include "phantomledger/transfers/fraud/injector.hpp"

#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/dispatch.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
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

Injector::Injector(Services services) noexcept : Injector(services, Rules{}) {}

Injector::Injector(Services services, Rules rules) noexcept
    : services_(services), rules_(rules) {}

InjectionOutput Injector::inject(const FraudPopulation &population) const {
  return inject(population, LegitCounterparties{});
}

InjectionOutput Injector::inject(const FraudPopulation &population,
                                 LegitCounterparties counterparties) const {
  // Defensive: a missing required input would otherwise produce a confusing
  // dereference deeper in the pipeline.
  if (services_.rng == nullptr) {
    throw std::invalid_argument(
        "Fraud injection requires an Injector::Services.rng");
  }
  if (population.topology == nullptr || population.accounts == nullptr ||
      population.ownership == nullptr) {
    throw std::invalid_argument(
        "Fraud injection requires topology, accounts and ownership");
  }
  if (population.profile == nullptr) {
    throw std::invalid_argument(
        "Fraud injection requires a FraudPopulation.profile");
  }

  // Empty-rings shortcut: the scenario passes through unchanged.
  if (population.topology->rings.empty()) {
    return InjectionOutput{
        .txns = std::vector<transactions::Transaction>(
            population.baseTxns.begin(), population.baseTxns.end()),
        .injectedCount = 0,
    };
  }

  // Materialise the execution context. Factory rebinds onto the injector's
  // RNG and reuses the router + ring infra so generated fraud rows carry
  // the same device/IP signatures the legit pipeline produces for ring
  // accounts.
  Execution execution{
      .txf = transactions::Factory(*services_.rng, services_.router,
                                   services_.ringInfra),
      .rng = services_.rng,
  };

  ActiveWindow window{
      .startDate = population.window.start,
      .days = population.window.days,
  };

  AccountPools pools{
      .allAccounts = {},
      .billerAccounts =
          std::vector<entity::Key>(counterparties.billerAccounts.begin(),
                                   counterparties.billerAccounts.end()),
      .employers = std::vector<entity::Key>(counterparties.employers.begin(),
                                            counterparties.employers.end()),
  };
  pools.allAccounts.reserve(population.accounts->records.size());
  for (const auto &record : population.accounts->records) {
    pools.allAccounts.push_back(record.id);
  }

  CamouflageContext camoCtx{
      .execution = execution,
      .window = window,
      .accounts = &pools,
      .rates = rules_.camouflage,
  };

  IllicitContext illicitCtx{
      .execution = execution,
      .window = window,
      .billerAccounts = std::span<const entity::Key>(
          pools.billerAccounts.data(), pools.billerAccounts.size()),
      .layeringRules = rules_.layering,
      .structuringRules = rules_.structuring,
  };

  // Build all ring plans up front. Plans are cheap (small vectors of Keys)
  // and we iterate them twice: once for camouflage, once for illicit.
  std::vector<Plan> ringPlans;
  ringPlans.reserve(population.topology->rings.size());
  for (const auto &ring : population.topology->rings) {
    ringPlans.push_back(buildPlan(ring, *population.topology,
                                  *population.accounts, *population.ownership));
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
      static_cast<double>(population.profile->limits.targetIllicitP),
      static_cast<std::int64_t>(population.baseTxns.size() + camoTxns.size()));

  std::vector<transactions::Transaction> out;
  out.reserve(
      population.baseTxns.size() + camoTxns.size() +
      static_cast<std::size_t>(std::max<std::int64_t>(0, targetIllicit)));
  out.assign(population.baseTxns.begin(), population.baseTxns.end());

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
    const auto typology = rules_.typology.choose(*services_.rng);
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
