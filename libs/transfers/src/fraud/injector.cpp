#include "phantomledger/transfers/fraud/injector.hpp"

#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/playbook.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/dispatch.hpp"
#include "phantomledger/transfers/fraud/typologies/unauthorized.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::transfers::fraud {

namespace {

[[nodiscard]] inline std::string ringStreamKey(std::uint32_t value) {
  char buf[16];
  auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
  (void)ec;
  return std::string(buf, ptr);
}

[[nodiscard]] std::int64_t toInt64Count(std::size_t value, const char *label) {
  constexpr auto kMax =
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());

  if (value > kMax) {
    throw std::overflow_error(std::string(label) +
                              " exceeds the supported int64 count range");
  }

  return static_cast<std::int64_t>(value);
}

[[nodiscard]] std::int64_t addCount(std::int64_t base, std::size_t extra,
                                    const char *label) {
  const auto extra64 = toInt64Count(extra, label);

  if (base > std::numeric_limits<std::int64_t>::max() - extra64) {
    throw std::overflow_error(std::string(label) +
                              " exceeds the supported int64 count range");
  }

  return base + extra64;
}

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

[[nodiscard]] Execution makeExecution(InjectorServices services,
                                      const random::RngFactory &factory) {
  return Execution{
      .txf = transactions::Factory(services.rng, services.router,
                                   services.ringInfra),
      .rng = &services.rng,
      .factory = &factory,
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

  auto *savedRng = ctx.execution.rng;
  const auto *keyFactory = ctx.execution.factory;

  for (const auto &plan : plans) {
    auto camoRng =
        keyFactory->rng({"fraud", "ring", ringStreamKey(plan.ringId), "camo"});

    ctx.execution.rng = &camoRng;
    ctx.execution.txf = ctx.execution.txf.rebound(camoRng);

    auto produced = camouflage::generate(ctx, plan, rates);

    out.insert(out.end(), std::make_move_iterator(produced.begin()),
               std::make_move_iterator(produced.end()));
  }

  ctx.execution.rng = savedRng;
  ctx.execution.txf = ctx.execution.txf.rebound(*savedRng);

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

    const bool isFinal = phaseIdx + 1 == phaseCount;

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

  const typologies::Dispatcher dispatcher{
      ctx,
      behavior,
  };

  auto *savedRng = ctx.execution.rng;
  const auto *keyFactory = ctx.execution.factory;

  for (std::int64_t ringIdx = 0; ringIdx < totalRings; ++ringIdx) {
    if (remainingBudget <= 0) {
      break;
    }

    const auto ringId = plans[static_cast<std::size_t>(ringIdx)].ringId;

    auto ringRng = keyFactory->rng({"fraud", "ring", ringStreamKey(ringId)});

    ctx.execution.rng = &ringRng;
    ctx.execution.txf = ctx.execution.txf.rebound(ringRng);

    ctx.seedChainIds(ringId);

    const auto perRing = ringBudget(remainingBudget, totalRings - ringIdx);

    const auto &playbook = behavior.playbooks.choose(ringRng);

    auto produced =
        runRingPlaybook(dispatcher, plans[static_cast<std::size_t>(ringIdx)],
                        perRing, playbook);

    remainingBudget -= static_cast<std::int64_t>(produced.size());

    out.insert(out.end(), std::make_move_iterator(produced.begin()),
               std::make_move_iterator(produced.end()));
  }

  ctx.execution.rng = savedRng;
  ctx.execution.txf = ctx.execution.txf.rebound(*savedRng);

  return out;
}

[[nodiscard]] InjectionOutput
assembleOutput(std::vector<transactions::Transaction> &&camoTxns,
               std::vector<transactions::Transaction> &&illicitTxns,
               std::vector<transactions::Transaction> &&unauthorizedTxns) {
  for (auto &txn : illicitTxns) {
    txn.fraud.type = ::PhantomLedger::fraud::FraudType::launderRing;
  }

  // unauthorized::generate types its own rows (txn_fraud_solo for the
  // card/ato rails, scam_gift_card for the victim-authorized rail) and
  // leaves its flag-0 reimbursement credits untyped — no blanket
  // assignment here (scam-fraud-2026-07).

  auto injected = std::move(camoTxns);

  injected.reserve(injected.size() + illicitTxns.size() +
                   unauthorizedTxns.size());

  injected.insert(injected.end(), std::make_move_iterator(illicitTxns.begin()),
                  std::make_move_iterator(illicitTxns.end()));

  injected.insert(injected.end(),
                  std::make_move_iterator(unauthorizedTxns.begin()),
                  std::make_move_iterator(unauthorizedTxns.end()));

  return InjectionOutput{
      .injected = std::move(injected),
  };
}

[[nodiscard]]
std::vector<typologies::unauthorized::CompromisePlan>
buildCompromisePlans(random::Rng &rng, time::Window window,
                     const entity::account::Registry &registry,
                     const entity::account::Ownership &ownership,
                     std::span<const Plan> ringPlans, std::int64_t budget) {
  using typologies::unauthorized::Rail;

  std::vector<typologies::unauthorized::CompromisePlan> plans;

  if (budget <= 0) {
    return plans;
  }

  std::unordered_set<entity::Key> excluded;

  for (const auto &ring : ringPlans) {
    excluded.insert(ring.fraudAccounts.begin(), ring.fraudAccounts.end());

    excluded.insert(ring.shellFraudAccounts.begin(),
                    ring.shellFraudAccounts.end());

    excluded.insert(ring.muleAccounts.begin(), ring.muleAccounts.end());

    excluded.insert(ring.victimAccounts.begin(), ring.victimAccounts.end());
  }

  const auto offsets = ownership.byPersonOffset.size();

  const std::size_t personLimit = offsets >= 2 ? offsets - 1 : 0;

  if (personLimit == 0) {
    return plans;
  }

  const detail::AccountView view{
      registry,
      ownership,
  };

  const auto pickAccount = [&](entity::Key avoid) -> entity::Key {
    for (int attempt = 0; attempt < 32; ++attempt) {
      const auto person =
          static_cast<entity::PersonId>(1 + rng.choiceIndex(personLimit));

      if (ownership.byPersonOffset[person - 1] ==
          ownership.byPersonOffset[person]) {
        continue;
      }

      const auto key = detail::primaryKey(view, person);

      if (key != avoid && !excluded.contains(key)) {
        return key;
      }
    }

    return avoid;
  };

  const auto windowStart = window.start.time_since_epoch().count();

  const auto usableDays = std::max(1, window.days - 8);

  std::int64_t remaining = budget;
  std::uint64_t seq = 0;

  while (remaining > 0) {
    // Rail mix (scam-fraud-2026-07): card .60 / gift-card scam .12 /
    // ato .28 — the scam share carved from the card rail, anchored to
    // the FTC CSN payment-method report mix (docs/fraud_model_audit.md
    // F-4). One uniform, same draw count as the old coin.
    const double railDraw = rng.nextDouble();
    const Rail rail = railDraw < 0.60
                          ? Rail::card
                          : (railDraw < 0.72 ? Rail::giftCardScam : Rail::ato);

    // Events per case: card U{5..14} is a documented CHOICE (dense
    // compromises for label density — per-case spend runs ~8x the UK
    // per-case average; F-4); ato U{3..8} is calibrated so per-case
    // drain ~= $3.0k against the UK remote-banking ~$3.5k/case
    // (card-behavior-2026-07); scam U{2..6} tracks the FTC coached
    // multi-card burst.
    const std::int64_t targetSpan =
        rail == Rail::card
            ? 5 + static_cast<std::int64_t>(rng.choiceIndex(10))
            : rail == Rail::giftCardScam
                  ? 2 + static_cast<std::int64_t>(rng.choiceIndex(5))
                  : 3 + static_cast<std::int64_t>(rng.choiceIndex(6));

    const auto target =
        static_cast<std::int32_t>(std::min<std::int64_t>(remaining, targetSpan));

    const auto victim = pickAccount(entity::Key{});

    if (victim == entity::Key{}) {
      break;
    }

    entity::Key drop{};

    if (rail == Rail::ato) {
      drop = pickAccount(victim);

      if (drop == victim) {
        break;
      }
    }

    const auto startDay = static_cast<std::int64_t>(
        rng.choiceIndex(static_cast<std::size_t>(usableDays)));

    const auto intraDay =
        3600 + static_cast<std::int64_t>(rng.nextDouble() * 72000.0);

    // Card compromises play out over days; ATO drains over hours; a
    // gift-card scam is ONE coached burst of 1-4 hours (the victim is
    // on the phone with the scammer the whole time).
    const auto spanSeconds = static_cast<std::int32_t>(
        3600 * (rail == Rail::card
                    ? 6 + rng.choiceIndex(66)
                    : rail == Rail::giftCardScam ? 1 + rng.choiceIndex(4)
                                                 : 2 + rng.choiceIndex(30)));

    plans.push_back(typologies::unauthorized::CompromisePlan{
        .victimAccount = victim,
        .dropAccount = drop,
        .device =
            devices::Identity{
                .ownerType = devices::OwnerType::ring,
                .ownerId = 0xACE00000ULL + seq,
                .slot = 0,
            },
        .ip = network::Ipv4::pack(198, 51, 100,
                                  static_cast<std::uint8_t>(1 + seq % 250)),
        .rail = rail,
        .startTs = windowStart + startDay * 86400 + intraDay,
        .spanSeconds = spanSeconds,
        .targetEvents = target,
        .seq = static_cast<std::uint32_t>(seq),
    });

    remaining -= target;
    ++seq;
  }

  return plans;
}

} // namespace

Injector::Injector(InjectorServices services, InjectorRingView rings,
                   InjectorAccountView accounts,
                   const Behavior &behavior) noexcept
    : services_(services), rings_(rings), accounts_(accounts),
      behavior_(behavior), fraudFactory_(services.fraudSeed) {}

InjectionOutput
Injector::inject(time::Window window,
                 std::span<const transactions::Transaction> baseTxns) const {
  return inject(window, baseTxns.size(), InjectorLegitCounterparties{});
}

InjectionOutput
Injector::inject(time::Window window,
                 std::span<const transactions::Transaction> baseTxns,
                 InjectorLegitCounterparties counterparties) const {
  return inject(window, baseTxns.size(), counterparties);
}

InjectionOutput
Injector::inject(time::Window window, std::size_t realizedBaseCount,
                 InjectorLegitCounterparties counterparties) const {
  requireInjectorPointers(rings_, accounts_);

  if (rings_.topology->rings.empty()) {
    return {};
  }

  const auto realizedBaseCount64 =
      toInt64Count(realizedBaseCount, "realized pre-fraud candidate count");

  Execution execution = makeExecution(services_, fraudFactory_);

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

  const auto illicitBudgetBase = addCount(realizedBaseCount64, camoTxns.size(),
                                          "illicit fraud budget base");

  const auto targetIllicit = calculateIllicitBudget(
      static_cast<double>(rings_.profile->limits.targetIllicitP),
      illicitBudgetBase);

  auto illicitTxns = generateIllicit(
      illicitCtx, behavior_, std::span<const Plan>(ringPlans), targetIllicit);

  const auto txnFraudBudgetBase = addCount(
      illicitBudgetBase, illicitTxns.size(), "transaction-fraud budget base");

  const auto txnFraudBudget = calculateIllicitBudget(
      static_cast<double>(rings_.profile->limits.targetTxnFraudP),
      txnFraudBudgetBase);

  auto unauthorizedPlannerRng =
      fraudFactory_.rng({"fraud", "unauth", "planner"});

  const auto compromisePlans = buildCompromisePlans(
      unauthorizedPlannerRng, window, *accounts_.registry, *accounts_.ownership,
      std::span<const Plan>(ringPlans), txnFraudBudget);

  auto unauthorizedTxns = typologies::unauthorized::generate(
      illicitCtx,
      std::span<const typologies::unauthorized::CompromisePlan>(
          compromisePlans),
      txnFraudBudget);

  return assembleOutput(std::move(camoTxns), std::move(illicitTxns),
                        std::move(unauthorizedTxns));
}

} // namespace PhantomLedger::transfers::fraud
