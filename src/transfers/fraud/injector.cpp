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

Injector::Injector(Services services, RingView rings,
                   AccountView accounts) noexcept
    : Injector(services, rings, accounts, Patterns{}) {}

Injector::Injector(Services services, RingView rings, AccountView accounts,
                   Patterns patterns) noexcept
    : services_(services), rings_(rings), accounts_(accounts),
      patterns_(patterns) {}

InjectionOutput
Injector::inject(time::Window window,
                 std::span<const transactions::Transaction> baseTxns) const {
  return inject(window, baseTxns, LegitCounterparties{});
}

InjectionOutput
Injector::inject(time::Window window,
                 std::span<const transactions::Transaction> baseTxns,
                 LegitCounterparties counterparties) const {
  if (rings_.topology == nullptr || accounts_.registry == nullptr ||
      accounts_.ownership == nullptr) {
    throw std::invalid_argument(
        "Fraud injection requires topology, accounts and ownership");
  }
  if (rings_.profile == nullptr) {
    throw std::invalid_argument(
        "Fraud injection requires a non-null RingView.profile");
  }

  if (rings_.topology->rings.empty()) {
    return InjectionOutput{
        .txns = std::vector<transactions::Transaction>(baseTxns.begin(),
                                                       baseTxns.end()),
        .injectedCount = 0,
    };
  }

  Execution execution{
      .txf = transactions::Factory(services_.rng, services_.router,
                                   services_.ringInfra),
      .rng = &services_.rng,
  };

  ActiveWindow activeWindow{
      .startDate = window.start,
      .days = window.days,
  };

  AccountPools pools{
      .allAccounts = {},
      .billerAccounts =
          std::vector<entity::Key>(counterparties.billerAccounts.begin(),
                                   counterparties.billerAccounts.end()),
      .employers = std::vector<entity::Key>(counterparties.employers.begin(),
                                            counterparties.employers.end()),
  };
  pools.allAccounts.reserve(accounts_.registry->records.size());
  for (const auto &record : accounts_.registry->records) {
    pools.allAccounts.push_back(record.id);
  }

  CamouflageContext camouflageCtx{
      .execution = execution,
      .window = activeWindow,
      .accounts = &pools,
      .rates = patterns_.camouflage,
  };

  IllicitContext illicitCtx{
      .execution = execution,
      .window = activeWindow,
      .billerAccounts = std::span<const entity::Key>(
          pools.billerAccounts.data(), pools.billerAccounts.size()),
      .layeringRules = patterns_.layering,
      .structuringRules = patterns_.structuring,
  };

  std::vector<Plan> ringPlans;
  ringPlans.reserve(rings_.topology->rings.size());
  for (const auto &ring : rings_.topology->rings) {
    ringPlans.push_back(buildPlan(ring, *rings_.topology, *accounts_.registry,
                                  *accounts_.ownership));
  }

  std::vector<transactions::Transaction> camoTxns;
  for (const auto &plan : ringPlans) {
    auto produced = camouflage::generate(camouflageCtx, plan);
    camoTxns.insert(camoTxns.end(), std::make_move_iterator(produced.begin()),
                    std::make_move_iterator(produced.end()));
  }

  const auto targetIllicit = calculateIllicitBudget(
      static_cast<double>(rings_.profile->limits.targetIllicitP),
      static_cast<std::int64_t>(baseTxns.size() + camoTxns.size()));

  std::vector<transactions::Transaction> out;
  out.reserve(
      baseTxns.size() + camoTxns.size() +
      static_cast<std::size_t>(std::max<std::int64_t>(0, targetIllicit)));
  out.assign(baseTxns.begin(), baseTxns.end());

  if (targetIllicit <= 0) {
    out.insert(out.end(), std::make_move_iterator(camoTxns.begin()),
               std::make_move_iterator(camoTxns.end()));
    return InjectionOutput{
        .txns = std::move(out),
        .injectedCount = camoTxns.size(),
    };
  }

  std::vector<transactions::Transaction> illicitTxns;
  illicitTxns.reserve(static_cast<std::size_t>(targetIllicit));

  std::int64_t remainingBudget = targetIllicit;
  const auto totalRings = static_cast<std::int64_t>(ringPlans.size());

  for (std::int64_t ringIdx = 0; ringIdx < totalRings; ++ringIdx) {
    if (remainingBudget <= 0) {
      break;
    }

    const auto perRing = ringBudget(remainingBudget, totalRings - ringIdx);
    const auto typology = patterns_.typology.choose(services_.rng);
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
