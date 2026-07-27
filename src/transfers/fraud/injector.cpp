#include "phantomledger/transfers/fraud/injector.hpp"

#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/entities/infra/random_ips.hpp"
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

// H3 part 3c-ii (authority U-8 addendum): rings never recruit the
// dead. Each plan's typology bursts and camouflage window clamp to
// the ring's alive horizon (min death over fraud + mule participants)
// minus this guard — the invoice typology's weekly lattice can emit
// up to 21 days past its base range (baseOffset < days-14 plus up to
// five 7-day steps); every other typology's tail padding already
// contains its bursts, and the chain typologies extend only by
// minutes/hours. Victims and the solo/unauthorized rail are exempt
// (deceased-account fraud is a real typology — declared).
inline constexpr int kRingScheduleGuardDays = 22;

[[nodiscard]] time::Window ringWindow(time::Window window,
                                      std::int64_t aliveEndEpoch) noexcept {
  const auto startEpoch = time::toEpochSeconds(window.start);
  const auto endEpoch =
      startEpoch + static_cast<std::int64_t>(window.days) * 86'400;
  if (aliveEndEpoch >= endEpoch) {
    return window; // every participant outlives the window
  }
  const auto aliveDays =
      static_cast<int>(std::max<std::int64_t>(0, aliveEndEpoch - startEpoch) /
                       86'400);
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
        buildPlan(ring, *rings.topology, registry, ownership,
                  rings.timelines));
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

    // H3: cover flows stop with the ring's alive horizon — the camo
    // lane is per-ring, so the clamp cannot move any other ring.
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

    // H3: the ring's bursts land inside its alive horizon — every
    // typology reads ctx.window exactly once (its burst sample), and
    // the ring rng lane is isolated, so the clamp cannot move any
    // other ring's stream.
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

  // unauthorized::generate types its own rows (txn_fraud_solo for the
  // card/ato rails, scam_gift_card / scam_impostor for the two
  // victim-authorized rails) and leaves its flag-0 reimbursement credits
  // untyped — no blanket assignment here (scam-fraud-2026-07,
  // victimization-v3).

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
buildCompromisePlans(
    random::Rng &rng, time::Window window,
    const entity::account::Registry &registry,
    const entity::account::Ownership &ownership,
    std::span<const Plan> ringPlans, std::int64_t budget,
    std::span<const entity::geography::GeoAreaId> homeAreas = {},
    // victimization-v2 (docs/card_fraud_victimization.md D1): per-person
    // card-exposure weights, PersonId-1 indexed, mean ~1.0. EMPTY keeps
    // the pre-v2 uniform draw bit-identical.
    std::span<const double> cardExposure = {},
    // victimization-v3 (docs/card_fraud_victimization.md D2): the
    // personas pack — persona-at-date, age-at-date and the join cohort.
    // The SCAM rails select on a persona x age hazard rebuilt AT THE
    // CASE DATE, their loss is age-graded, and EVERY rail now requires
    // bank membership at the case date. nullptr keeps the v2 behaviour.
    const synth::personas::Pack *personas = nullptr) {
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

  // card-fraud-realism-v2 step b: the picker now yields the OWNER as
  // well as the account key. The victim's person id is the only thing
  // that maps to a home area, and it is already in hand at selection
  // time — carrying it out is free and avoids a key->person reverse
  // index that does not exist. Draw order and draw COUNT are
  // unchanged by this, so the pick itself is stream-neutral.
  struct PickedAccount {
    entity::Key key{};
    entity::PersonId person = 0;
  };

  // victimization-v2 + v3: VICTIM SELECTION IS PER RAIL, because the
  // two fraud families recruit victims by different mechanisms. Mixing
  // them was v2's remaining defect: its flat floor existed so that
  // nobody became unreachable by social engineering, which is
  // SCAM-rail reasoning that was also nudging the card rail.
  //
  //   card / ato  EXPOSURE (exposure.hpp). Unauthorized third-party
  //               fraud finds you through your card and account
  //               activity. Date-independent, so the CDF is built ONCE.
  //   scam rails  PERSONA x AGE SUSCEPTIBILITY (susceptibility.hpp),
  //               rebuilt AT THE CASE DATE. An impostor scam arrives by
  //               phone; exposure is the wrong axis entirely. Evaluating
  //               at the case date is what makes the life course
  //               visible — over a 30-year window a person ages out of
  //               the incidence gradient and into the severity one.
  //   drops       UNIFORM (see the ato/impostor branch below).
  //
  // Both weighted paths spend exactly one uniform per attempt, like the
  // choiceIndex draw they replace, and each rail degrades independently
  // to that draw when its carrier is absent.
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

  // The scam hazard is DATE-dependent, so it is cached on the last case
  // date and rebuilt when the date changes. The build is O(population)
  // and consumes NO randomness, so it cannot move a stream however
  // often it runs.
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

      // MEMBERSHIP AT THE CASE DATE (victimization-v3). A person who has
      // not joined yet has no account to compromise or to push money out
      // of, so no rail may reach them before their join date.
      //
      // DEATH IS RAIL-DEPENDENT, by declaration. The scam rails pass
      // requireAlive: a dead person cannot be talked into authorizing a
      // payment — that is not a typology, it is an impossibility. The
      // card and ATO rails do NOT, which PRESERVES the existing declared
      // position that deceased-account fraud is real (see the ring
      // schedule guard above, authority U-8 addendum) instead of
      // silently reversing it.
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

  // The victim's home area, or invalidGeoArea when the carrier is
  // absent / the person is out of range. invalidGeoArea is exactly the
  // "no local anchor" value the spending session already falls back on,
  // so an unfilled carrier degrades to the pre-v2 national behavior
  // rather than to something undefined.
  const auto homeAreaOf =
      [&](entity::PersonId person) -> entity::geography::GeoAreaId {
    if (person == 0 || homeAreas.empty() ||
        static_cast<std::size_t>(person) > homeAreas.size()) {
      return entity::geography::invalidGeoArea;
    }
    return homeAreas[static_cast<std::size_t>(person) - 1];
  };

  const auto windowStart = window.start.time_since_epoch().count();

  const auto usableDays = std::max(1, window.days - 8);

  std::int64_t remaining = budget;
  std::uint64_t seq = 0;

  while (remaining > 0) {
    // Rail mix (victimization-v3): card .48 / gift-card scam .12 /
    // impostor push .12 / ato .28. The impostor-push share is carved
    // from the card rail exactly as the gift-card share was in
    // scam-fraud-2026-07, and the two authorized rails carry EQUAL
    // weight: FTC CSN names gift cards the most-REPORTED scam payment
    // method of the era and bank transfers the largest by reported
    // LOSS, so an even split is the declared CHOICE that keeps both
    // mechanisms present. One uniform, same draw count as before.
    const double railDraw = rng.nextDouble();
    const Rail rail =
        railDraw < 0.48
            ? Rail::card
            : (railDraw < 0.60
                   ? Rail::giftCardScam
                   : (railDraw < 0.72 ? Rail::scamImpostor : Rail::ato));

    const bool scamRail = typologies::unauthorized::authorizedRail(rail);

    // Events per case: card U{5..14} is a documented CHOICE (dense
    // compromises for label density — per-case spend runs ~8x the UK
    // per-case average; F-4); ato U{3..8} is calibrated so per-case
    // drain ~= $3.0k against the UK remote-banking ~$3.5k/case
    // (card-behavior-2026-07); the gift-card scam U{2..6} tracks the
    // FTC coached multi-card burst; the impostor push is U{1..3},
    // because an authorized-push case is FEW transfers of LARGE size —
    // that contrast with the card rail is the point of having both.
    //
    // THE EVENT COUNT IS NOT AGE-GRADED (victimization-v3, revised).
    // An earlier revision scaled the gift-card burst by the victim's
    // severity multiplier, which put an 80-year-old at up to 13 cards —
    // $6,500 of gift cards inside four hours, out of a retail checking
    // account. The research supports LOSS rising with age, and the
    // impostor rail carries that with a continuous amount; manufacturing
    // bursts the victim cannot fund only produces rows the ledger
    // discards. Severity now applies to the impostor AMOUNT alone.
    const std::int64_t targetSpan =
        rail == Rail::card
            ? 5 + static_cast<std::int64_t>(rng.choiceIndex(10))
            : rail == Rail::giftCardScam
                  ? 2 + static_cast<std::int64_t>(rng.choiceIndex(5))
                  : rail == Rail::scamImpostor
                        ? 1 + static_cast<std::int64_t>(rng.choiceIndex(3))
                        : 3 + static_cast<std::int64_t>(rng.choiceIndex(6));

    const auto target = static_cast<std::int32_t>(
        std::min<std::int64_t>(remaining, targetSpan));

    // THE CASE DATE IS DRAWN BEFORE THE VICTIM (victimization-v3).
    // A scam happens at a time and then finds someone susceptible AT
    // THAT TIME. Persona and age are not person constants in this model
    // — that is the whole point of the H2/H3 timeline work — so the
    // victim-first order could not express a life course at all. It is
    // also what lets every rail require membership at the case date.
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

    // AGE-GRADED SEVERITY, the IMPOSTOR rail only (susceptibility.hpp:
    // FTC CSN median reported loss rises ~3x from the youngest age band
    // to the oldest). It rides the plan into the amount sampler. The
    // gift-card rail is deliberately ungraded: a rack caps a card at
    // $500 whoever is buying, so the denomination cannot carry it, and
    // grading the card COUNT instead manufactured unfundable bursts
    // (see targetSpan above).
    const double severity =
        rail == Rail::scamImpostor && victimPick.person != 0
            ? victims.severity(
                  static_cast<std::size_t>(victimPick.person) - 1, startPoint)
            : 1.0;

    entity::Key drop{};

    if (rail == Rail::ato || rail == Rail::scamImpostor) {
      // UNIFORM, deliberately: the ATO drop and the impostor payee are
      // accounts the ATTACKER controls. Neither exposure nor
      // susceptibility has any bearing on being one, so weighting them
      // would apply victim-side reasoning to the wrong end of the edge.
      // Membership still binds — a payee account has to exist.
      drop = pickFrom(victim, nullptr, startPoint, false).key;

      if (drop == victim) {
        break;
      }
    }

    // Card compromises play out over days; ATO drains over hours; a
    // gift-card scam is ONE coached burst of 1-4 hours (the victim is
    // on the phone with the scammer the whole time); an impostor push
    // runs 1-6 hours — the call, then the branch visit or the app.
    const auto spanSeconds = static_cast<std::int32_t>(
        3600 * (rail == Rail::card
                    ? 6 + rng.choiceIndex(66)
                    : rail == Rail::giftCardScam
                          ? 1 + rng.choiceIndex(4)
                          : rail == Rail::scamImpostor
                                ? 1 + rng.choiceIndex(6)
                                : 2 + rng.choiceIndex(30)));

    // SHORTCUT #2, CLOSED (card-fraud-realism-v2 step b).
    //
    // The attacker session's address used to be a hardcoded
    // 198.51.100.x — TEST-NET-2, a RESERVED documentation range — while
    // legitimate sessions draw `network::randomIpv4` over first octets
    // 11-222 across the full 32-bit space. A legitimate address landing
    // in that /24 has probability ~1 in 14 million, so an IP-prefix
    // lookup labelled unauthorized fraud essentially perfectly: a model
    // could score at ceiling while learning nothing about behavior.
    // That is the same class of defect as the merchant-identity
    // shortcut this arc exists to close, and the online-GNN audit
    // flagged it.
    //
    // Attacker addresses now come from the SAME distribution as
    // legitimate ones. The intended fraud signal is BEHAVIORAL and
    // survives untouched: ring participants still share infrastructure
    // through SharedInfra, and a compromise still concentrates its
    // events on one address — what disappears is only the free lookup
    // on the address itself.
    //
    // STILL OPEN: `.device.ownerId` below carries a magic 0xACE00000
    // prefix with OwnerType::ring. Whether that leaks depends on what
    // the exporters actually render (renderDeviceId may hash the key,
    // in which case the prefix is invisible downstream) — that check is
    // the remaining half of this finding, tracked in
    // docs/card_fraud_v2_roadmap.md step d. On the AUTHORIZED rails the
    // attacker session is wrong for a second reason: the operator is
    // the victim on their own device (authority U-12, declared).
    plans.push_back(typologies::unauthorized::CompromisePlan{
        .victimAccount = victim,
        .dropAccount = drop,
        .device =
            devices::Identity{
                .ownerType = devices::OwnerType::ring,
                .ownerId = 0xACE00000ULL + seq,
                .slot = 0,
            },
        .ip = network::randomIpv4(rng),
        .rail = rail,
        .startTs = startTs,
        .spanSeconds = spanSeconds,
        .targetEvents = target,
        .seq = static_cast<std::uint32_t>(seq),
        .homeArea = homeAreaOf(victimPick.person),
        .severity = severity,
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

  // victimization-v1 (docs/card_fraud_victimization.md F1): there is
  // deliberately NO blanket ring guard here.
  //
  // Ring topology governs the ORGANIZED-CRIME families only, and each
  // of those already guards itself: generateCamouflage loops over the
  // plan span (empty span -> no rows) and generateIllicit returns early
  // on an empty one. The UNAUTHORIZED family — card compromise, the
  // gift-card scam, ATO — has no dependence on rings whatsoever:
  // buildCompromisePlans draws victims across the WHOLE roster
  // (excluding ring participants, so the two populations stay
  // disjoint), the attacker is exogenous by construction (source is the
  // victim's own account, destination a catalogue merchant, IP a random
  // address), and the budget rides targetTxnFraudP x the realized
  // candidate count.
  //
  // The blanket `if (rings.empty()) return {};` that used to sit here
  // therefore silenced SCAMS in any world too small to plan a ring —
  // ring count is round(lognormal x population/1e4) with no floor, i.e.
  // ZERO below roughly population 833. A 500-person 30-year corpus came
  // out with no fraud of any kind. Ring absence at small population is
  // realistic; ring absence silencing an exogenous-attacker scam is
  // not.
  //
  // For any world WITH at least one ring this function is bit-identical
  // to the guarded version — which is why the fix is golden-neutral.
  // tests/test_fraud_low_population.cpp pins both halves: fraud exists
  // at a ring-free population, and it is EXCLUSIVELY the exogenous
  // family (zero launder_ring rows).

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
      // card-fraud-realism-v2 step b: the merchant acceptance catalogue
      // and the home-area axis the card rails select on.
      .merchants = counterparties.merchants,
      .homeAreas = counterparties.homeAreas,
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
      std::span<const Plan>(ringPlans), txnFraudBudget,
      counterparties.homeAreas, counterparties.cardExposure,
      counterparties.personas);

  auto unauthorizedTxns = typologies::unauthorized::generate(
      illicitCtx,
      std::span<const typologies::unauthorized::CompromisePlan>(
          compromisePlans),
      txnFraudBudget);

  return assembleOutput(std::move(camoTxns), std::move(illicitTxns),
                        std::move(unauthorizedTxns));
}

} // namespace PhantomLedger::transfers::fraud
