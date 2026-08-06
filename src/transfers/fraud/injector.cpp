#include "phantomledger/transfers/fraud/injector.hpp"

#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/playbook.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/susceptibility.hpp"
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

/* Rings never recruit the dead (authority U-8 addendum): every plan's typology
 * bursts and camouflage window clamp to the ring's alive horizon (min death
 * over fraud + mule participants) minus this guard. 22 days because the
 * invoice typology's weekly lattice can emit up to 21 days past its base range
 * (baseOffset < days-14, plus up to five 7-day steps); every other typology's
 * tail padding already contains its bursts and the chain typologies extend
 * only by minutes/hours. Victims and the solo/unauthorized rail are exempt —
 * deceased-account fraud is a declared, real typology. */
inline constexpr int kRingScheduleGuardDays = 22;

[[nodiscard]] time::Window ringWindow(time::Window window,
                                      std::int64_t aliveEndEpoch) noexcept {
  const auto startEpoch = time::toEpochSeconds(window.start);
  const auto endEpoch =
      startEpoch + static_cast<std::int64_t>(window.days) * 86'400;
  if (aliveEndEpoch >= endEpoch) {
    return window; // every participant outlives the window
  }
  const auto aliveDays = static_cast<int>(
      std::max<std::int64_t>(0, aliveEndEpoch - startEpoch) / 86'400);
  window.days =
      std::max(1, std::min(window.days, aliveDays - kRingScheduleGuardDays));
  return window;
}

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
buildRingPlans(const InjectorRingView &rings,
               const entity::account::Registry &registry,
               const entity::account::Ownership &ownership) {
  std::vector<Plan> plans;
  plans.reserve(rings.topology->rings.size());

  for (const auto &ring : rings.topology->rings) {
    plans.push_back(
        buildPlan(ring, *rings.topology, registry, ownership, rings.timelines));
  }

  return plans;
}

[[nodiscard]] std::vector<transactions::Transaction>
generateCamouflage(CamouflageContext &ctx, std::span<const Plan> plans,
                   const camouflage::Rates &rates) {
  std::vector<transactions::Transaction> out;

  auto *savedRng = ctx.execution.rng;
  const auto *keyFactory = ctx.execution.factory;
  const auto fullWindow = ctx.window;

  for (const auto &plan : plans) {
    auto camoRng =
        keyFactory->rng({"fraud", "ring", ringStreamKey(plan.ringId), "camo"});

    ctx.execution.rng = &camoRng;
    ctx.execution.txf = ctx.execution.txf.rebound(camoRng);

    /* Cover flows stop at the ring's alive horizon. The camo lane is
     * per-ring, so the clamp cannot move any other ring. */
    ctx.window = ringWindow(fullWindow, plan.participantsAliveEndEpoch);

    auto produced = camouflage::generate(ctx, plan, rates);

    out.insert(out.end(), std::make_move_iterator(produced.begin()),
               std::make_move_iterator(produced.end()));
  }

  ctx.window = fullWindow;
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
  const auto fullWindow = ctx.window;

  for (std::int64_t ringIdx = 0; ringIdx < totalRings; ++ringIdx) {
    if (remainingBudget <= 0) {
      break;
    }

    const auto &plan = plans[static_cast<std::size_t>(ringIdx)];
    const auto ringId = plan.ringId;

    auto ringRng = keyFactory->rng({"fraud", "ring", ringStreamKey(ringId)});

    ctx.execution.rng = &ringRng;
    ctx.execution.txf = ctx.execution.txf.rebound(ringRng);

    ctx.seedChainIds(ringId);

    /* The ring's bursts land inside its alive horizon. Every typology reads
     * ctx.window exactly once (its burst sample) and the ring rng lane is
     * isolated, so the clamp cannot move any other ring's stream. */
    ctx.window = ringWindow(fullWindow, plan.participantsAliveEndEpoch);

    const auto perRing = ringBudget(remainingBudget, totalRings - ringIdx);

    const auto &playbook = behavior.playbooks.choose(ringRng);

    auto produced = runRingPlaybook(dispatcher, plan, perRing, playbook);

    remainingBudget -= static_cast<std::int64_t>(produced.size());

    out.insert(out.end(), std::make_move_iterator(produced.begin()),
               std::make_move_iterator(produced.end()));
  }

  ctx.window = fullWindow;
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

  /* No blanket assignment for the unauthorized rows: unauthorized::generate
   * types its own (txn_fraud_solo for card/ato, scam_gift_card /
   * scam_impostor for the two victim-authorized rails) and deliberately
   * leaves its flag-0 reimbursement credits untyped. */

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

/* Share of UNAUTHORIZED (card / ATO) cases operated from the VICTIM'S OWN
 * endpoint rather than exogenous attacker infrastructure: a banking trojan or
 * remote-access tool driving the victim's session, and household / "friendly"
 * fraud on the victim's own device.
 *
 * IT IS ALSO WHAT MAKES THE OWNERSHIP TOPOLOGY EXPORTABLE. With every fraud
 * row on exogenous infrastructure and every legitimate row on a customer
 * endpoint, "endpoint not associated with any Party" is a perfect fraud rule
 * and `Has_Device`/`Has_IP` have to be withheld wholesale. This branch
 * supplies fraud that genuinely sits on an on-file customer endpoint; the
 * complementary direction — legitimate rows on endpoints the institution has
 * NOT recorded — comes from `infra::enrollment` coverage.
 *
 * DECLARED CHOICE, DIRECTION ANCHORED: malware/RAT-driven and household card
 * fraud is a real but MINORITY share of unauthorized loss. The realized share
 * also absorbs cases with no live operator campaign at the case date, so it is
 * measured, not assumed — tests/test_card_endpoint_graph.cpp prints both
 * components and bands the total. */
inline constexpr double kVictimEndpointShare = 0.18;

/* Share of operator sessions exiting through a RESIDENTIAL CUSTOMER ADDRESS
 * instead of the operator's own. Residential-proxy resale is a documented
 * staple of card-not-present fraud: the exit address is a real consumer line,
 * which is why address reputation alone does not classify it. This is the
 * IP-side counterpart of the branch above and the source of the graph's most
 * interesting structure — a fraud transaction reachable from an unrelated
 * Party through a shared address node. */
inline constexpr double kResidentialProxyShare = 0.30;

[[nodiscard]] std::vector<typologies::unauthorized::CompromisePlan>
buildCompromisePlans(
    random::Rng &rng, time::Window window,
    const entity::account::Registry &registry,
    const entity::account::Ownership &ownership,
    std::span<const Plan> ringPlans, std::int64_t budget,
    /* The exogenous infrastructure pool, and the router the
     * residential-proxy branch borrows a customer address from. Both
     * nullptr-tolerant; see InjectorServices::attackers. */
    const infra::AttackerInfra *attackers = nullptr,
    const infra::Router *router = nullptr,
    std::span<const entity::geography::GeoAreaId> homeAreas = {},
    /* Per-person card-exposure weights, PersonId-1 indexed, mean ~1.0
     * (docs/card_fraud_victimization.md D1). EMPTY keeps the uniform draw
     * bit-identical. */
    std::span<const double> cardExposure = {},
    /* The personas pack — persona-at-date, age-at-date and the join cohort
     * (docs/card_fraud_victimization.md D2). The SCAM rails select on a
     * persona x age hazard rebuilt AT THE CASE DATE, their loss is age-graded,
     * and EVERY rail requires bank membership at the case date. nullptr stands
     * all three down. */
    const synth::personas::Pack *personas = nullptr,
    /* The victim's home-area HISTORY. This pass plans the whole window up
     * front, so it needs the home AT THE CASE DATE, not a current value.
     * nullptr means homes never move. */
    const entity::parties::relocation::Schedule *relocation = nullptr) {
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

  /* The picker yields the OWNER as well as the account key: the person id is
   * the only thing that maps to a home area, and it is already in hand at
   * selection time, which avoids a key->person reverse index that does not
   * exist. Carrying it out changes neither draw order nor draw COUNT. */
  struct PickedAccount {
    entity::Key key{};
    entity::PersonId person = 0;
  };

  /* VICTIM SELECTION IS PER RAIL, because the two fraud families recruit by
   * different mechanisms. Do not merge them behind one picker: a flat floor
   * that keeps anyone reachable by social engineering is SCAM-rail reasoning,
   * and applying it to the card rail nudges the wrong axis.
   *
   *   card / ato  EXPOSURE (exposure.hpp). Unauthorized third-party fraud
   *               finds you through your card and account activity.
   *               Date-independent, so the CDF is built ONCE.
   *   scam rails  PERSONA x AGE SUSCEPTIBILITY (susceptibility.hpp), rebuilt
   *               AT THE CASE DATE. An impostor scam arrives by phone;
   *               exposure is the wrong axis entirely. Evaluating at the case
   *               date is what makes the life course visible — over a 30-year
   *               window a person ages out of the incidence gradient and into
   *               the severity one.
   *   drops       UNIFORM (see the ato/impostor branch below).
   *
   * Both weighted paths spend exactly one uniform per attempt, the same as the
   * plain choiceIndex draw, and each rail degrades independently to that draw
   * when its carrier is absent. */
  struct Cdf {
    std::vector<double> prefix;
    double total = 0.0;

    [[nodiscard]] bool usable() const noexcept {
      return !prefix.empty() && total > 0.0;
    }
  };

  const auto makeCdf = [personLimit](std::span<const double> weights) -> Cdf {
    Cdf cdf;
    if (weights.empty()) {
      return cdf;
    }
    cdf.prefix.reserve(personLimit);
    for (std::size_t i = 0; i < personLimit; ++i) {
      const double w = i < weights.size() ? weights[i] : 1.0;
      cdf.total += w > 0.0 ? w : 0.0;
      cdf.prefix.push_back(cdf.total);
    }
    if (!(cdf.total > 0.0)) {
      cdf.prefix.clear();
      cdf.total = 0.0;
    }
    return cdf;
  };

  const Cdf exposure = makeCdf(cardExposure);

  /* The scam hazard is DATE-dependent, so it is cached on the last case date
   * and rebuilt when the date changes. The build is O(population) and consumes
   * NO randomness, so it cannot move a stream however often it runs. */
  const susceptibility::VictimPopulation victims{personas, window.start};
  std::vector<double> scamWeightBuf;
  Cdf scamCdf;
  bool scamCached = false;
  std::int64_t scamCachedTs = 0;

  const auto scamCdfAt = [&](std::int64_t ts) -> const Cdf & {
    if (!scamCached || ts != scamCachedTs) {
      scamWeightBuf =
          victims.scamWeights(time::fromEpochSeconds(ts), personLimit);
      scamCdf = makeCdf(scamWeightBuf);
      scamCachedTs = ts;
      scamCached = true;
    }
    return scamCdf;
  };

  const auto pickFrom = [&](entity::Key avoid, const Cdf *cdf,
                            time::TimePoint at,
                            bool requireAlive) -> PickedAccount {
    for (int attempt = 0; attempt < 32; ++attempt) {
      entity::PersonId person = 0;
      if (cdf != nullptr && cdf->usable()) {
        const double u = rng.nextDouble() * cdf->total;
        const auto hit =
            std::lower_bound(cdf->prefix.begin(), cdf->prefix.end(), u);
        const auto idx = static_cast<std::size_t>(hit - cdf->prefix.begin());
        person =
            static_cast<entity::PersonId>(1 + std::min(idx, personLimit - 1));
      } else {
        person =
            static_cast<entity::PersonId>(1 + rng.choiceIndex(personLimit));
      }

      if (ownership.byPersonOffset[person - 1] ==
          ownership.byPersonOffset[person]) {
        continue;
      }

      /* MEMBERSHIP AT THE CASE DATE. A person who has not joined yet has no
       * account to compromise or to push money out of, so no rail may reach
       * them before their join date.
       *
       * DEATH IS RAIL-DEPENDENT, by declaration. The scam rails pass
       * requireAlive — a dead person cannot be talked into authorizing a
       * payment, which is an impossibility rather than a typology. The card
       * and ATO rails do NOT, which keeps deceased-account fraud real (see the
       * ring schedule guard above, authority U-8 addendum) instead of silently
       * reversing it. */
      if (!victims.member(static_cast<std::size_t>(person) - 1, at,
                          requireAlive)) {
        continue;
      }

      const auto key = detail::primaryKey(view, person);

      if (key != avoid && !excluded.contains(key)) {
        return PickedAccount{key, person};
      }
    }

    return PickedAccount{avoid, 0};
  };

  /* The home the victim occupied AT THE CASE TIMESTAMP, or invalidGeoArea when
   * the carrier is absent or the person is out of range. invalidGeoArea is the
   * same "no local anchor" value the spending session falls back on, so an
   * unfilled carrier degrades to the national behaviour rather than to
   * something undefined.
   *
   * IT MUST TAKE THE CASE TIMESTAMP, not a current value: this pass plans the
   * whole window before the fold runs, so a case in year 12 must see the
   * year-12 home. A window-start snapshot would attribute every case in a
   * 20-year run to the victim's original area.
   *
   * DRAW-FREE — a schedule lookup, no uniform — so it cannot move the stream
   * position; only the geographic ANSWER changes. */
  const auto homeAreaOf = [&](entity::PersonId person,
                              std::int64_t ts) -> entity::geography::GeoAreaId {
    if (person == 0) {
      return entity::geography::invalidGeoArea;
    }
    if (relocation != nullptr && !relocation->empty()) {
      const auto area = relocation->areaAt(person, ts);
      if (entity::geography::validArea(area)) {
        return area;
      }
    }
    if (homeAreas.empty() ||
        static_cast<std::size_t>(person) > homeAreas.size()) {
      return entity::geography::invalidGeoArea;
    }
    return homeAreas[static_cast<std::size_t>(person) - 1];
  };

  const auto windowStart = window.start.time_since_epoch().count();

  const auto usableDays = std::max(1, window.days - 8);

  std::int64_t remaining = budget;
  std::uint64_t seq = 0;
  std::size_t consecutiveBoundaryRejects = 0;
  constexpr std::size_t kMaxBoundaryResamples = 4'096;

  while (remaining > 0) {
    /* Rail mix: card .48 / gift-card scam .12 / impostor push .12 / ato .28.
     * The two AUTHORIZED rails carry EQUAL weight as a declared CHOICE that
     * keeps both mechanisms present — FTC CSN names gift cards the
     * most-REPORTED scam payment method of the era and bank transfers the
     * largest by reported LOSS. One uniform. */
    const double railDraw = rng.nextDouble();
    const Rail rail =
        railDraw < 0.48
            ? Rail::card
            : (railDraw < 0.60
                   ? Rail::giftCardScam
                   : (railDraw < 0.72 ? Rail::scamImpostor : Rail::ato));

    const bool scamRail = typologies::unauthorized::authorizedRail(rail);

    /* Events per case. card U{5..14} is a documented CHOICE — dense
     * compromises for label density, so per-case spend runs ~8x the UK
     * per-case average (audit F-4). ato U{3..8} is calibrated so per-case
     * drain ~= $3.0k against the UK remote-banking ~$3.5k/case. The gift-card
     * scam U{2..6} tracks the FTC coached multi-card burst. The impostor push
     * is U{1..3}, because an authorized-push case is FEW transfers of LARGE
     * size — that contrast with the card rail is the point of having both.
     *
     * THE EVENT COUNT IS NOT AGE-GRADED, and do not make it so. Scaling the
     * gift-card burst by the victim's severity multiplier puts an 80-year-old
     * at up to 13 cards — $6,500 inside four hours out of a retail checking
     * account. Most of those rows are unfundable, so the ledger discards them
     * and the visible result is a decline burst, not a larger loss. Severity
     * applies to the impostor AMOUNT alone. */
    const std::int64_t targetSpan =
        rail == Rail::card ? 5 + static_cast<std::int64_t>(rng.choiceIndex(10))
        : rail == Rail::giftCardScam
            ? 2 + static_cast<std::int64_t>(rng.choiceIndex(5))
        : rail == Rail::scamImpostor
            ? 1 + static_cast<std::int64_t>(rng.choiceIndex(3))
            : 3 + static_cast<std::int64_t>(rng.choiceIndex(6));

    const auto target = static_cast<std::int32_t>(
        std::min<std::int64_t>(remaining, targetSpan));

    /* THE CASE DATE IS DRAWN BEFORE THE VICTIM, and the order is load-bearing.
     * A scam happens at a time and then finds someone susceptible AT THAT
     * TIME; persona and age are not person constants here, so a victim-first
     * order cannot express a life course at all. It is also what lets every
     * rail require membership at the case date. */
    const auto startDay = static_cast<std::int64_t>(
        rng.choiceIndex(static_cast<std::size_t>(usableDays)));

    const auto intraDay =
        3600 + static_cast<std::int64_t>(rng.nextDouble() * 72000.0);

    const auto startTs = windowStart + startDay * 86400 + intraDay;
    const auto startPoint = time::fromEpochSeconds(startTs);

    const Cdf *victimCdf = scamRail ? &scamCdfAt(startTs) : &exposure;

    const auto victimPick =
        pickFrom(entity::Key{}, victimCdf, startPoint, scamRail);
    const auto victim = victimPick.key;

    if (victim == entity::Key{}) {
      break;
    }

    /* AGE-GRADED SEVERITY, the IMPOSTOR rail only (susceptibility.hpp: FTC CSN
     * median reported loss rises ~3x from the youngest age band to the
     * oldest). It rides the plan into the amount sampler. The gift-card rail
     * is deliberately ungraded — a rack caps a card at $500 whoever is buying,
     * so the denomination cannot carry it, and grading the card COUNT instead
     * manufactures unfundable bursts (see targetSpan above). */
    const double severity =
        rail == Rail::scamImpostor && victimPick.person != 0
            ? victims.severity(static_cast<std::size_t>(victimPick.person) - 1,
                               startPoint)
            : 1.0;

    PickedAccount dropPick{};
    entity::Key drop{};

    if (rail == Rail::ato || rail == Rail::scamImpostor) {
      /* UNIFORM, deliberately: the ATO drop and the impostor payee are
       * accounts the ATTACKER controls. Neither exposure nor susceptibility
       * bears on being one, so weighting them would apply victim-side
       * reasoning to the wrong end of the edge. Membership still binds — a
       * payee account has to exist. */
      dropPick = pickFrom(victim, nullptr, startPoint, false);
      drop = dropPick.key;

      if (drop == victim) {
        break;
      }
    }

    /* Card compromises play out over days; ATO drains over hours; a gift-card
     * scam is ONE coached burst of 1-4 hours (the victim is on the phone with
     * the scammer throughout); an impostor push runs 1-6 hours — the call,
     * then the branch visit or the app. */
    const auto spanSeconds = static_cast<std::int32_t>(
        3600 * (rail == Rail::card           ? 6 + rng.choiceIndex(66)
                : rail == Rail::giftCardScam ? 1 + rng.choiceIndex(4)
                : rail == Rail::scamImpostor ? 1 + rng.choiceIndex(6)
                                             : 2 + rng.choiceIndex(30)));

    /* CASE-DATE ELIGIBILITY IS NOT ENOUGH: a compromise expands into events up
     * to 71 hours later, so resolve the earliest exclusive participant
     * boundary. Authorized rails stop at victim death; every rail stops at
     * victim account closure; account-to-account rails also stop if the
     * receiving customer closes.
     *
     * A case whose COMPLETE SAMPLED SPAN does not fit is REJECTED, never
     * compressed — squeezing all target events into a short residual interval
     * would manufacture a boundary-velocity shortcut. `remaining` is
     * deliberately unchanged, so the next iteration reselects a case and the
     * accepted plans still carry the requested budget. The resample ceiling
     * stops a pathological all-boundary carrier from looping forever. This
     * adds draws only for a rejected case, identically in both engines. */
    auto eventEndTsExclusive = victims.eligibleUntilEpoch(
        static_cast<std::size_t>(victimPick.person) - 1, scamRail);
    if (dropPick.person != entity::invalidPerson && dropPick.person != 0) {
      eventEndTsExclusive =
          std::min(eventEndTsExclusive,
                   victims.eligibleUntilEpoch(
                       static_cast<std::size_t>(dropPick.person) - 1,
                       /*requireAlive=*/false));
    }
    if (eventEndTsExclusive != std::numeric_limits<std::int64_t>::max() &&
        (eventEndTsExclusive <= startTs ||
         eventEndTsExclusive - startTs <
             static_cast<std::int64_t>(spanSeconds))) {
      if (++consecutiveBoundaryRejects >= kMaxBoundaryResamples) {
        break; // pathological carrier: no full case fits this population
      }
      continue;
    }
    consecutiveBoundaryRejects = 0;

    /* ATTACKER ENDPOINTS COME FROM `infra::AttackerInfra` — campaigns with a
     * lifetime, concurrent device/IP lines that TILE the campaign, and a
     * Pareto case load so a few operators work many cards. Minting a fresh
     * device and address per case makes cross-victim endpoint sharing zero by
     * construction, which deletes "one endpoint touching many cards": the
     * single most valuable signal a card-fraud graph carries, and the reason
     * to model this as a graph rather than a table.
     *
     * ATTACKER ADDRESSES MUST STAY ON THE SAME DISTRIBUTION AS LEGITIMATE
     * ONES. A fixed or reserved range (e.g. TEST-NET-2 198.51.100.x, against
     * legitimate `network::randomIpv4` over first octets 11-222) makes an
     * IP-prefix lookup label unauthorized fraud essentially perfectly — a
     * model scores at ceiling while learning nothing about behaviour. The
     * intended signal is BEHAVIOURAL and survives without it: ring
     * participants share infrastructure through SharedInfra, and a compromise
     * still concentrates its events on one address.
     *
     * `.device.ownerId` below is a deterministic INTERNAL identity, so the
     * events of one compromise share infrastructure. The exporter hashes the
     * complete Identity into the same fixed-width opaque `D...` namespace used
     * by legitimate devices, so neither the sentinel value nor OwnerType::ring
     * reaches an exported id.
     *
     * FOUR UNIFORMS, ALWAYS, IN THIS FIXED ORDER, DRAWN UNCONDITIONALLY so no
     * branch can change the count — that is what confines endpoint changes to
     * the device_id / ip_address columns and leaves every rail, event count,
     * amount and timestamp bit-identical. The two AUTHORIZED rails burn all
     * four and discard them (~24% of mints are dead weight); that is
     * deliberate, because reclaiming them would re-roll every subsequent plan
     * for no modelling gain. */
    const double operatorU = rng.nextDouble();
    const double proxyU = rng.nextDouble();
    const double proxyPickU = rng.nextDouble();
    const double victimEndpointU = rng.nextDouble();

    devices::Identity attackerDevice{};
    network::Ipv4 attackerIp{};

    /* Resolve the CAMPAIGN once, ABOVE the endpoint branch, so it is available
     * whether or not this case gets attacker infrastructure. DRAW-FREE:
     * `operatorAt` is a const point query over the `operatorU` already drawn,
     * so the four-uniform block stays unconditional and ordered.
     *
     * IT MUST STAY ABOVE THE BRANCH, which is what DECOUPLES merchant sharing
     * from device sharing: campaign coverage then spans every unauthorized
     * case with a live operator rather than the device-resolved subset. If the
     * two covered the same case set they would be collinear, and a GNN could
     * not tell a shared cash-out venue from a shared device. */
    std::uint32_t campaign =
        typologies::unauthorized::CompromisePlan::kNoCampaign;
    if (!scamRail && attackers != nullptr && !attackers->empty()) {
      if (const auto op = attackers->operatorAt(operatorU, startTs)) {
        campaign = *op;
      }
    }

    /* THE CARRIER IS THE ABSENCE OF A CARRIER. An UNASSIGNED device on the
     * plan means "the victim operated this row", and `unauthorized::generate`
     * then lets the session `transactions::Factory` already routed for the
     * victim stand. Do not add a flag for this: there is no second field to
     * keep in sync, and hand-built unit plans that DO carry an endpoint keep
     * their existing meaning. */
    if (!scamRail && victimEndpointU >= kVictimEndpointShare &&
        attackers != nullptr && !attackers->empty()) {
      /* DRAW-FREE FROM HERE DOWN, and it must stay that way. Resolution is a
       * point query against world state at the case date: which campaign was
       * running, and which of its live endpoints. `seq` salts the choice among
       * CONCURRENTLY-live lines, so one operator's cases spread across its
       * machines while a single case keeps one endpoint for all its events — a
       * case has one modus operandi.
       *
       * The horizon is the case's EXCLUSIVE event end, because
       * `unauthorized::generate` places every event in [startTs, startTs +
       * span) and an endpoint must cover the whole interval to be attributable
       * to all of them. Resolving on the case START alone would let a late
       * event carry an endpoint already replaced by then.
       *
       * `max(..., 2)` MIRRORS the generator's own span floor exactly.
       * Production spans are hours so the clamp never binds, but a horizon
       * computed from the raw value would be a silent off-by-one waiting for
       * the first caller with a degenerate span — and it would surface as a
       * point-in-time violation rather than an error. */
      const auto caseEndTsExcl =
          startTs + std::max<std::int64_t>(2, spanSeconds);
      /* Reuses the campaign resolved above rather than re-querying. Same value
       * by construction (same `operatorU`, same `startTs`, `operatorAt` is
       * const), so endpoint attribution is unchanged. */
      if (campaign != typologies::unauthorized::CompromisePlan::kNoCampaign) {
        const auto op = campaign;
        const auto device =
            attackers->deviceAt(op, startTs, caseEndTsExcl, seq);
        auto ip = attackers->ipAt(op, startTs, caseEndTsExcl, seq);

        /* RESIDENTIAL PROXY: exit through some customer's home address.
         * `liveIpFor` is the draw-free, sticky-free point query and must stay
         * one — advancing the borrowed customer's own routing state would make
         * their later legitimate rows depend on whether an attacker passed
         * through, differently in each engine. */
        if (proxyU < kResidentialProxyShare && router != nullptr &&
            personLimit > 0) {
          const auto idx =
              std::min(static_cast<std::size_t>(
                           proxyPickU * static_cast<double>(personLimit)),
                       personLimit - 1);
          const auto proxyPerson = static_cast<entity::PersonId>(1 + idx);
          if (const auto borrowed =
                  router->liveIpFor(proxyPerson, startTs, caseEndTsExcl, seq)) {
            ip = borrowed;
          }
        }

        /* BOTH LEGS OR NEITHER. A half-resolved session puts an attacker
         * device beside a victim address on the same row, which is not a
         * typology — it is a bug wearing one. */
        if (device.has_value() && ip.has_value()) {
          attackerDevice = *device;
          attackerIp = *ip;
        }
      }
    }

    plans.push_back(typologies::unauthorized::CompromisePlan{
        /* Unauthorized card rows deliberately stay on the primary account's
         * derived debit-card instrument. Fraud is planned only after the
         * legitimate CardCycleDriver has closed and serviced its credit-card
         * cycles, so pointing a late-injected purchase at a modeled
         * credit-card liability would bypass statements, payments, interest
         * and limit behaviour. Moving these rows to issued credit cards is a
         * lifecycle-ordering redesign, not a key swap. */
        .victimAccount = victim,
        .dropAccount = drop,
        .device = attackerDevice,
        .ip = attackerIp,
        .rail = rail,
        .startTs = startTs,
        .spanSeconds = spanSeconds,
        .targetEvents = target,
        .eventEndTsExclusive = eventEndTsExclusive,
        .seq = static_cast<std::uint32_t>(seq),
        .homeArea = homeAreaOf(victimPick.person, startTs),
        .severity = severity,
        .campaign = campaign,
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

  /* THERE IS DELIBERATELY NO BLANKET RING GUARD HERE — do not add
   * `if (rings.empty()) return {};`.
   *
   * Ring topology governs the ORGANIZED-CRIME families only, and each already
   * guards itself: generateCamouflage loops over the plan span (empty span ->
   * no rows) and generateIllicit returns early on an empty one. The
   * UNAUTHORIZED family — card compromise, gift-card scam, ATO — has no
   * dependence on rings at all: buildCompromisePlans draws victims across the
   * WHOLE roster (excluding ring participants, so the populations stay
   * disjoint), the attacker is exogenous by construction, and the budget rides
   * targetTxnFraudP x the realized candidate count.
   *
   * A blanket guard silences SCAMS in any world too small to plan a ring. Ring
   * count is round(lognormal x population/1e4) with no floor, i.e. ZERO below
   * roughly population 833, so a 500-person 30-year corpus comes out with no
   * fraud of any kind. tests/test_fraud_low_population.cpp pins both halves:
   * fraud exists at a ring-free population, and it is EXCLUSIVELY the
   * exogenous family (zero launder_ring rows). */

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
      /* The merchant acceptance catalogue and the home-area axis the card
       * rails select on. */
      .merchants = counterparties.merchants,
      .homeAreas = counterparties.homeAreas,
      /* Already in scope here, so wiring it moves no inject call site, no
       * harness carrier and no infra-stage signature. */
      .attackers = services_.attackers,
  };

  const auto ringPlans =
      buildRingPlans(rings_, *accounts_.registry, *accounts_.ownership);

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
      std::span<const Plan>(ringPlans), txnFraudBudget, services_.attackers,
      services_.router, counterparties.homeAreas, counterparties.cardExposure,
      counterparties.personas, counterparties.relocation);

  auto unauthorizedTxns = typologies::unauthorized::generate(
      illicitCtx,
      std::span<const typologies::unauthorized::CompromisePlan>(
          compromisePlans),
      txnFraudBudget);

  return assembleOutput(std::move(camoTxns), std::move(illicitTxns),
                        std::move(unauthorizedTxns));
}

} // namespace PhantomLedger::transfers::fraud
