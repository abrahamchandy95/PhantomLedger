//
// tests/test_membership.cpp
//
// macro-history-v1 H3 part 3c-ii: MEMBERSHIP [joinTs, closeTs), the
// BEA-sized JOIN COHORT, the joiner AGE-AXIS repair, ACCOUNT-CLOSURE
// stops for the contractual flows, the ATM/internal death stops, and
// the fraud-scheduling intervals (contract docs/h3_mortality_estate.md,
// authority U-8 + addendum). Three parts:
//
//   A. The join primitive (pure): determinism, BEA-consistent sizing,
//      in-window placement, declining-growth skew (early years recruit
//      more).
//   B. Pack anchors (entities stage): the seed roster is BYTE-STABLE
//      against the join cohort (same dob/timeline draws with and
//      without joinDays); joiners' ages draw AT THE JOIN DATE; death
//      lands strictly after joining; personaAt(anchor) == seed; the
//      Membership view's [join, close) semantics; the AML end-of-window
//      closure resolution.
//   C. A full 300-person 4-year base-routine world WITH the join
//      cohort: ATM + internal-transfer death stops (strict), the
//      subscription closure stop + estate-servicing window + the H1
//      CPI DEFECT FIX (every debit deflates back onto the kPricePool
//      lattice), premium/obligation closure stops + the claims death
//      stop, card servicing truncated before closure, ring fraud never
//      recruiting the dead, and the standard exporter's membership
//      filter dropping pre-join joiner rows while post-closure drops
//      stay ZERO (generation already stopped — the declared invariant).
//
// MODEL-MOVING round: the four goldens recapture with it.
//

#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/standard/membership_filter.hpp"
#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/synth/econ/catalog.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/synth/personas/join.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"
#include "phantomledger/transfers/channels/insurance/rates.hpp"
#include "phantomledger/transfers/channels/subscriptions/prices.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include "gate_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace pl = ::PhantomLedger;
namespace joins = pl::synth::personas::join_cohort;
namespace tlx = pl::synth::personas::timeline;
namespace econ = pl::synth::econ;
namespace channels = pl::channels;
namespace xfer = pl::pipeline::stages::transfers;
namespace fraud_ns = pl::transfers::fraud;
namespace pii = pl::synth::pii;

using pl::entity::person::Flag;
using pltest::GateWorld;
using pltest::WorldSpec;
using Txn = pl::transactions::Transaction;

namespace {

constexpr std::uint64_t kSeed = 424242;
constexpr std::int32_t kPopulation = 300;
constexpr int kDays = 1460;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

[[nodiscard]] pl::time::Window canonicalWindow(int days) {
  return pl::time::Window{
      .start = pl::time::makeTime(
          pl::time::CalendarDate{.year = 1991, .month = 1, .day = 1}),
      .days = days,
  };
}

[[nodiscard]] std::int64_t epochOf(pl::time::TimePoint tp) {
  return pl::time::toEpochSeconds(tp);
}

// ------------------------------------------------------------- Part A

void partA_joinPrimitive() {
  const auto window = canonicalWindow(10'592); // the canonical 29 years
  constexpr std::size_t kN = 10'000;

  const auto a = joins::deriveJoinDays(kSeed, kN, window);
  const auto b = joins::deriveJoinDays(kSeed, kN, window);
  check(a == b, "join-day derivation deterministic");
  check(a.size() == kN, "join-day carrier covers the population");

  const auto joiners = joins::joinerCount(kN, window);
  check(joiners > 0 && joiners < kN, "canonical window has a join cohort");
  check(joins::joinerCount(kN, pl::time::Window{.start = window.start,
                                                .days = 0}) == 0,
        "empty window has no joiners");

  // Independent sizing reference: the compound population index over
  // the window's calendar years (the linear day-weighted model must
  // land inside a loose band around it).
  const auto &m = econ::macroSeries();
  const double g =
      m.at(std::min(2020, m.lastYear())).populationThousands /
      m.at(1991).populationThousands;
  const double frac = static_cast<double>(joiners) / static_cast<double>(kN);
  check(frac > 0.5 * (g - 1.0) && frac < 1.1 * (g - 1.0),
        "joiner fraction tracks the BEA population index (got " +
            std::to_string(frac) + " vs index growth " +
            std::to_string(g - 1.0) + ")");

  // Placement: every joiner day in-window; seeds all zero; declining
  // growth rates put MORE than half the cohort in the first half.
  std::size_t inFirstHalf = 0;
  std::size_t joinerSeen = 0;
  const std::size_t firstJoiner = kN - joiners + 1;
  for (std::size_t p = 1; p <= kN; ++p) {
    const auto day = a[p - 1];
    if (p < firstJoiner) {
      check(day == 0, "seed person has join day 0");
      continue;
    }
    ++joinerSeen;
    check(day < static_cast<std::uint32_t>(window.days),
          "join day inside the window");
    if (day < static_cast<std::uint32_t>(window.days) / 2) {
      ++inFirstHalf;
    }
  }
  check(joinerSeen == joiners, "joiner tail size matches joinerCount");
  const double earlyShare =
      static_cast<double>(inFirstHalf) / static_cast<double>(joinerSeen);
  check(earlyShare > 0.52,
        "declining growth skews join days early (first-half share " +
            std::to_string(earlyShare) + ")");

  std::printf("  part A: joiners %zu of %zu (%.1f%%), first-half share "
              "%.3f\n",
              joiners, kN, frac * 100.0, earlyShare);
}

// ------------------------------------------------------------- Part B

struct Packs {
  pl::synth::people::Pack people;
  pl::synth::personas::Pack personas;
};

[[nodiscard]] Packs buildPack(const pl::synth::pii::PoolSet &pools,
                              const pl::time::Window &window,
                              bool withJoinCohort) {
  namespace entityStage = pl::pipeline::stages::entities;
  auto rng = pl::random::Rng::fromSeed(kSeed);
  const pl::synth::pii::IdentityContext identity{
      .pools = &pools,
      .simStart = window.start,
      .localeMix = pl::synth::pii::LocaleMix::usOnly(),
      .windowDays = withJoinCohort ? window.days : 0,
  };
  Packs out;
  out.people = entityStage::buildPeople(rng, 400, {});
  out.personas = entityStage::buildPersonas(rng, out.people, identity);
  return out;
}

void partB_anchors(const pl::synth::pii::PoolSet &pools) {
  const auto window = canonicalWindow(10'592);
  const auto control = buildPack(pools, window, /*withJoinCohort=*/false);
  const auto cohort = buildPack(pools, window, /*withJoinCohort=*/true);

  const auto &joinDays = cohort.personas.joinDays;
  const auto n = cohort.personas.assignment.byPerson.size();
  check(joinDays.size() == n, "pack carries the join carrier");

  std::size_t joiners = 0;
  std::size_t seedsIdentical = 0;
  std::size_t seedsTotal = 0;
  bool sawJoinerDobShift = false;

  for (std::size_t i = 0; i < n; ++i) {
    const auto &cd = control.personas.birthDates[i];
    const auto &jd = cohort.personas.birthDates[i];
    const auto &ct = control.personas.timelines[i];
    const auto &jt = cohort.personas.timelines[i];

    const auto anchor =
        window.start + pl::time::Days{static_cast<int>(joinDays[i])};

    if (joinDays[i] == 0) {
      // Seed roster (or a day-0 joiner): the anchor is unchanged, so
      // dob/timeline/mortality draws must be byte-identical.
      ++seedsTotal;
      const bool dobSame =
          cd.year == jd.year && cd.month == jd.month && cd.day == jd.day;
      const bool tlSame = ct.retirement == jt.retirement &&
                          ct.workStart == jt.workStart &&
                          ct.businessEnd == jt.businessEnd &&
                          ct.death == jt.death && ct.male == jt.male;
      if (dobSame && tlSame) {
        ++seedsIdentical;
      }
      continue;
    }

    ++joiners;

    if (!(cd.year == jd.year && cd.month == jd.month && cd.day == jd.day)) {
      sawJoinerDobShift = true;
    }

    // The age axis: a joiner's age is drawn AT THE JOIN DATE.
    const auto dobTs = pl::time::makeTime(pl::time::CalendarDate{
        .year = jd.year, .month = jd.month, .day = jd.day});
    const double ageAtJoin =
        static_cast<double>(epochOf(anchor) - epochOf(dobTs)) /
        (365.2425 * 86'400.0);
    check(ageAtJoin > 15.0 && ageAtJoin < 106.0,
          "joiner age at JOIN inside the persona bands (got " +
              std::to_string(ageAtJoin) + ")");

    // Alive at join: death strictly after the join anchor.
    check(jt.death > anchor, "joiner dies strictly after joining");

    // The pinned seed-consistency invariant, at the person's own anchor.
    check(tlx::personaAt(jt, anchor) == jt.seed,
          "personaAt(join anchor) == seed for joiners");
  }

  check(joiners > 0, "canonical pack has joiners");
  check(seedsTotal > 0 && seedsIdentical == seedsTotal,
        "seed roster byte-stable against the join cohort (" +
            std::to_string(seedsIdentical) + " of " +
            std::to_string(seedsTotal) + ")");
  check(sawJoinerDobShift,
        "the age-axis repair is observable (some joiner dob moved)");

  // ---- The membership interval view.
  const auto membership = joins::membershipOf(cohort.personas, window);
  const auto startEpoch = epochOf(window.start);
  const auto endEpoch = epochOf(window.endExcl());

  std::size_t closedInWindow = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto person = static_cast<pl::entity::PersonId>(i + 1);
    const auto joinEpoch =
        startEpoch + static_cast<std::int64_t>(joinDays[i]) * 86'400;
    const auto closeEpoch =
        epochOf(cohort.personas.timelines[i].death) +
        static_cast<std::int64_t>(pii::kSettlementDays) * 86'400;

    if (joinDays[i] > 0) {
      check(!membership.activeAt(person, joinEpoch - 1),
            "joiner inactive the second before joinTs");
    }
    if (joinEpoch < closeEpoch) {
      check(membership.activeAt(person, joinEpoch),
            "member active at joinTs");
    }
    check(!membership.activeAt(person, closeEpoch),
          "member inactive at closeTs (account closed)");

    const auto closedAt = membership.closedAt(person);
    if (closeEpoch < endEpoch) {
      ++closedInWindow;
      check(closedAt != pl::time::TimePoint{} &&
                epochOf(closedAt) == closeEpoch,
            "closedAt reports the in-window closure instant");
    } else {
      check(closedAt == pl::time::TimePoint{},
            "closedAt empty while still open at window end");
    }
  }
  check(closedInWindow > 0, "the 29-year window closes some accounts");

  // Out-of-range owners (counterparties, hubs) are always active.
  check(membership.activeAt(0, startEpoch) &&
            membership.activeAt(static_cast<pl::entity::PersonId>(n + 50),
                                startEpoch),
        "non-person owners always active");

  // ---- The AML end-of-window closure resolution.
  pl::exporter::aml::vertices::SharedContext ctx;
  ctx.personaByPerson = cohort.personas.assignment.byPerson;
  pl::exporter::aml::vertices::resolveEndOfWindowPersonas(
      ctx, cohort.personas, endEpoch - 1, /*rows=*/1);
  check(ctx.closedByPerson.size() == n, "closure resolution covers everyone");
  std::size_t resolverClosed = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto closeEpoch =
        epochOf(cohort.personas.timelines[i].death) +
        static_cast<std::int64_t>(pii::kSettlementDays) * 86'400;
    const bool expect = (endEpoch - 1) >= closeEpoch;
    check((ctx.closedByPerson[i] != 0) == expect,
          "AML customer status closed exactly at account closure");
    resolverClosed += ctx.closedByPerson[i] != 0 ? 1U : 0U;
  }
  check(resolverClosed == closedInWindow,
        "resolver and membership agree on the closed set");

  std::printf("  part B: joiners %zu of %zu, in-window closures %zu\n",
              joiners, n, closedInWindow);
}

// ------------------------------------------------------------- Part C

void partC_worldGates(const pl::synth::pii::PoolSet &pools) {
  WorldSpec spec;
  spec.seed = kSeed;
  spec.window = canonicalWindow(kDays);
  spec.population = kPopulation;
  spec.fraudProfile = pltest::scaledFraudProfile();
  spec.withIncome = true;
  spec.withBaseRoutines = true;
  spec.withProducts = true;
  spec.withJoinCohort = true;
  // Non-const: the fraud injector (C7) binds the world's shared rng.
  GateWorld world(pools, spec);

  const auto &timelines = world.people.personas.timelines;
  const auto &registry = world.holdings.accounts.registry;
  const auto startEpoch = epochOf(spec.window.start);
  const auto endEpoch = epochOf(spec.window.endExcl());

  std::unordered_map<pl::entity::Key, pl::entity::PersonId> ownerOfKey;
  ownerOfKey.reserve(registry.records.size());
  for (const auto &rec : registry.records) {
    ownerOfKey.emplace(rec.id, rec.owner);
  }

  const auto deathEpochOf = [&](pl::entity::PersonId p) -> std::int64_t {
    if (p == 0 || static_cast<std::size_t>(p) > timelines.size()) {
      return std::numeric_limits<std::int64_t>::max();
    }
    return epochOf(timelines[p - 1].death);
  };
  const auto closeEpochOf = [&](pl::entity::PersonId p) -> std::int64_t {
    const auto d = deathEpochOf(p);
    if (d == std::numeric_limits<std::int64_t>::max()) {
      return d;
    }
    return d + static_cast<std::int64_t>(pii::kSettlementDays) * 86'400;
  };
  const auto ownerOf = [&](const pl::entity::Key &k) -> pl::entity::PersonId {
    const auto it = ownerOfKey.find(k);
    return it == ownerOfKey.end() ? pl::entity::invalidPerson : it->second;
  };

  std::size_t deaths = 0;
  for (const auto &tl : timelines) {
    if (epochOf(tl.death) < endEpoch) {
      ++deaths;
    }
  }
  check(deaths > 0, "world carries in-window deaths");

  // ---- C1/C2/C3/C4: the base-routine stream (income + split + rent +
  // subscriptions + ATM + internal transfers).
  const auto atmTag = channels::tag(channels::Legit::atm);
  const auto selfTag = channels::tag(channels::Legit::selfTransfer);
  const auto subTag = channels::tag(channels::Legit::subscription);

  std::size_t atmRows = 0, atmDead = 0;
  std::size_t selfRows = 0, selfDead = 0;
  std::size_t subRows = 0, subPostClose = 0, subEstateWindow = 0;
  std::size_t subOffPool = 0;
  std::size_t subNonCalibrationYear = 0;

  // C4 (the H1 subscription CPI defect fix): every debit, deflated by
  // its own year's price scale, must land back on the kPricePool
  // lattice the contract's price was drawn from. Keying by
  // (source,target) would NOT work here: the biller pool is tiny (the
  // hub accounts), so one pair carries several different-priced
  // contracts. Tolerance covers cent rounding at 1991 scale (~0.53).
  const auto onPricePool = [](double deflated) {
    for (const double price : pl::transfers::subscriptions::kPricePool) {
      if (std::fabs(deflated - price) <= 0.02) {
        return true;
      }
    }
    return false;
  };

  for (const auto &t : world.streams.screened()) {
    const auto tag = t.session.channel;
    const auto srcOwner = ownerOf(t.source);

    if (tag == atmTag) {
      ++atmRows;
      if (t.timestamp >= deathEpochOf(srcOwner)) {
        ++atmDead;
      }
    } else if (tag == selfTag) {
      ++selfRows;
      if (t.timestamp >= deathEpochOf(srcOwner)) {
        ++selfDead;
      }
    } else if (tag == subTag) {
      ++subRows;
      if (t.timestamp >= closeEpochOf(srcOwner)) {
        ++subPostClose;
      }
      if (t.timestamp >= deathEpochOf(srcOwner) &&
          t.timestamp < closeEpochOf(srcOwner)) {
        ++subEstateWindow;
      }
      const int year =
          pl::time::toCalendarDate(pl::time::fromEpochSeconds(t.timestamp))
              .year;
      if (year != econ::macroSeries().calibrationYear()) {
        ++subNonCalibrationYear;
      }
      if (!onPricePool(t.amount / econ::priceScale(year))) {
        ++subOffPool;
      }
    }
  }

  check(atmRows > 100, "atm rows populated (" + std::to_string(atmRows) + ")");
  check(atmDead == 0, "the dead withdraw no cash (violations " +
                          std::to_string(atmDead) + ")");
  check(selfRows > 100,
        "internal-transfer rows populated (" + std::to_string(selfRows) + ")");
  check(selfDead == 0, "the dead move no money between their own accounts "
                       "(violations " +
                           std::to_string(selfDead) + ")");
  check(subRows > 500,
        "subscription rows populated (" + std::to_string(subRows) + ")");
  check(subPostClose == 0, "no subscription posts at/after account closure "
                           "(violations " +
                               std::to_string(subPostClose) + ")");
  check(subEstateWindow > 0,
        "the estate services subscriptions between death and closure (" +
            std::to_string(subEstateWindow) + " rows)");
  check(subNonCalibrationYear > 0,
        "the leg carries off-calibration-year debits — the CPI gate has "
        "teeth");
  check(subOffPool == 0,
        "subscription debits realize the CPI level (deflate back onto "
        "kPricePool; violations " +
            std::to_string(subOffPool) + " of " + std::to_string(subRows) +
            ")");

  // ---- C5: products — premiums/obligations stop at closure, claims
  // at death (the exact production emitter, product_replay.cpp).
  {
    const pl::random::RngFactory laneFactory{kSeed};
    auto productRng = laneFactory.rng({"products", "full_schedule"});
    const pl::transactions::Factory productTxf(productRng,
                                               &world.productRouter,
                                               &world.infra.ringInfra);
    const pl::pipeline::stages::products::ObligationSynthesis synthesis{};
    xfer::ProductTxnEmitter emitter{spec.window, kSeed,        productRng,
                                    productTxf,  world.people, synthesis};
    const auto primary = xfer::primaryAccounts(world.holdings);

    const auto premiums = emitter.premiums(world.holdings, primary);
    std::size_t premiumPostClose = 0, premiumEstateWindow = 0;
    for (const auto &t : premiums) {
      const auto owner = ownerOf(t.source);
      if (t.timestamp >= closeEpochOf(owner)) {
        ++premiumPostClose;
      }
      if (t.timestamp >= deathEpochOf(owner) &&
          t.timestamp < closeEpochOf(owner)) {
        ++premiumEstateWindow;
      }
    }
    check(!premiums.empty(), "premium rows populated");
    check(premiumPostClose == 0,
          "no premium posts at/after account closure (violations " +
              std::to_string(premiumPostClose) + ")");
    check(premiumEstateWindow > 0,
          "the estate services premiums between death and closure (" +
              std::to_string(premiumEstateWindow) + " rows)");

    const auto obligations = emitter.obligations(world.holdings, primary);
    std::size_t obligationPostClose = 0;
    for (const auto &t : obligations) {
      if (t.timestamp >= closeEpochOf(ownerOf(t.source))) {
        ++obligationPostClose;
      }
    }
    check(!obligations.empty(), "obligation rows populated");
    check(obligationPostClose == 0,
          "no loan/tax obligation posts at/after closure (violations " +
              std::to_string(obligationPostClose) + ")");

    const auto claims = emitter.claims(pl::transfers::insurance::ClaimRates{},
                                       world.holdings, primary);
    std::size_t claimPostDeath = 0;
    for (const auto &t : claims) {
      // Claims pay carrier -> payer: the PAYER (target) is the person.
      if (t.timestamp >= deathEpochOf(ownerOf(t.target))) {
        ++claimPostDeath;
      }
    }
    check(claimPostDeath == 0, "the dead file no insurance claims "
                               "(violations " +
                                   std::to_string(claimPostDeath) + ")");
  }

  // ---- C6: card servicing stops before closure. Feed every in-window
  // decedent's credit card a pre-death purchase burst, run the real
  // driver over the window, and require every emitted session row to
  // land strictly before the owner's closure.
  {
    std::unordered_map<pl::entity::Key, pl::entity::PersonId> deadCardOwner;
    std::vector<Txn> purchases;
    const auto purchaseTag = channels::tag(channels::Legit::cardPurchase);

    for (const auto &rec : world.holdings.creditCards.records) {
      const auto owner = rec.owner;
      const auto death = deathEpochOf(owner);
      if (death >= endEpoch) {
        continue; // survivor
      }
      if (death - startEpoch < 60LL * 86'400) {
        continue; // no room for a pre-death burst
      }
      deadCardOwner.emplace(rec.key, owner);
      for (int i = 0; i < 6; ++i) {
        Txn t{};
        t.source = rec.key;
        t.target = world.plan.counterparties().issuerAcct;
        t.amount = 150.0 + 10.0 * i;
        t.timestamp = startEpoch + (30LL + 3 * i) * 86'400 + 12 * 3'600;
        t.session.channel = purchaseTag;
        purchases.push_back(t);
      }
    }

    if (!deadCardOwner.empty()) {
      pl::transfers::credit_cards::DriverInputs inputs{
          .cards = &world.holdings.creditCards,
          .primaryAccounts = &world.cardCfg.primaryAccounts,
          .issuerAccount = world.cardCfg.issuerAccount,
          .window = spec.window,
          .timelines = timelines,
      };
      pl::transfers::credit_cards::CardCycleDriver driver{
          *world.cardCfg.rules, *world.txf, pl::random::RngFactory{kSeed},
          inputs, nullptr};
      driver.ingestPurchases(purchases);
      for (int d = 0; d < spec.window.days; ++d) {
        driver.tickDay(static_cast<std::uint32_t>(d),
                       spec.window.start + pl::time::Days{d});
      }
      driver.drainResidual();
      const auto emitted = driver.takeEmitted();

      std::size_t cardRows = 0, cardPostClose = 0;
      for (const auto &t : emitted) {
        auto it = deadCardOwner.find(t.source);
        if (it == deadCardOwner.end()) {
          it = deadCardOwner.find(t.target);
        }
        if (it == deadCardOwner.end()) {
          continue;
        }
        ++cardRows;
        if (t.timestamp >= closeEpochOf(it->second)) {
          ++cardPostClose;
        }
      }
      check(cardRows > 0, "decedent card sessions emit rows (statements "
                          "settle against the estate)");
      check(cardPostClose == 0,
            "no card session row posts at/after account closure "
            "(violations " +
                std::to_string(cardPostClose) + ")");
      std::printf("  part C: decedent cards %zu, session rows %zu\n",
                  deadCardOwner.size(), cardRows);
    } else {
      std::printf("  part C: note — no decedent carried a credit card at "
                  "this seed; card gate not exercised\n");
    }
  }

  // ---- C7: rings never recruit the dead (the injector with the
  // timeline carrier, mirroring TransferStage::makeFraudInjector).
  {
    xfer::FraudEmission fraudEmission;
    fraudEmission.profile(&world.fraudProfile);
    fraudEmission.behavior(&fraud_ns::kDefaultBehavior);

    const fraud_ns::Injector injector{
        fraud_ns::InjectorServices{
            .rng = world.rng,
            .router = &world.infra.router,
            .ringInfra = &world.infra.ringInfra,
            .fraudSeed = kSeed ^ 0x9E3779B97F4A7C15ULL,
        },
        fraudEmission.ringView(world.people.roster.topology,
                               world.people.personas.timelines),
        xfer::FraudEmission::accountView(world.holdings.accounts.registry,
                                         world.holdings.accounts.ownership),
        fraudEmission.resolvedBehavior(),
    };

    pl::transfers::legit::ledger::LegitCounterparties legitCps;
    legitCps.hubAccounts = world.plan.counterparties().hubAccounts;
    legitCps.billerAccounts = world.plan.counterparties().billerAccounts;
    legitCps.employers = world.plan.counterparties().employers;

    const auto injected = injector.inject(
        spec.window, world.streams.screened().size(),
        xfer::FraudEmission::legitCounterparties(legitCps));

    const auto &roster = world.people.roster.roster;
    std::size_t ringRows = 0, ringDead = 0;
    for (const auto &t : injected.injected) {
      if (t.fraud.flag == 0 ||
          t.fraud.type != pl::fraud::FraudType::launderRing) {
        continue; // solo/unauthorized rail exempt (declared)
      }
      ++ringRows;
      for (const auto &key : {t.source, t.target}) {
        const auto p = ownerOf(key);
        if (p == pl::entity::invalidPerson || p == 0 ||
            static_cast<std::size_t>(p) > timelines.size()) {
          continue;
        }
        const bool activeParticipant =
            roster.has(p, Flag::fraud) || roster.has(p, Flag::mule);
        if (activeParticipant && t.timestamp >= deathEpochOf(p)) {
          ++ringDead;
        }
      }
    }
    check(ringRows > 0, "ring fraud rows populated (" +
                            std::to_string(ringRows) + ")");
    check(ringDead == 0, "rings never recruit the dead (violations " +
                             std::to_string(ringDead) + ")");
  }

  // ---- C8: the standard exporter's membership filter over the base
  // stream: pre-join joiner rows drop; post-closure drops are ZERO
  // (generation already stopped — the declared invariant).
  {
    const auto membership =
        joins::membershipOf(world.people.personas, spec.window);
    const auto &joinDays = world.people.personas.joinDays;

    std::size_t droppedPreJoin = 0, droppedPostClose = 0;
    for (const auto &t : world.streams.screened()) {
      for (const auto &key : {t.source, t.target}) {
        const auto p = ownerOf(key);
        if (p == pl::entity::invalidPerson || p == 0 ||
            static_cast<std::size_t>(p) > joinDays.size()) {
          continue;
        }
        const auto joinEpoch =
            startEpoch + static_cast<std::int64_t>(joinDays[p - 1]) * 86'400;
        if (t.timestamp < joinEpoch) {
          ++droppedPreJoin;
          break;
        }
        if (t.timestamp >= closeEpochOf(p)) {
          ++droppedPostClose;
          break;
        }
      }
    }

    const auto visible = pl::exporter::standard::filterByMembership(
        world.streams.screened(), registry, world.holdings.accounts.lookup,
        membership);

    check(droppedPreJoin > 0,
          "joiners exist before joining — the join filter has work (" +
              std::to_string(droppedPreJoin) + " rows hidden)");
    check(droppedPostClose == 0,
          "nothing posts after closure — generation stopped before the "
          "filter must (violations " +
              std::to_string(droppedPostClose) + ")");
    check(visible.size() ==
              world.streams.screened().size() - droppedPreJoin -
                  droppedPostClose,
          "the membership filter hides exactly the out-of-interval rows");
    std::printf("  part C: deaths %zu, pre-join rows hidden %zu, visible "
                "%zu of %zu\n",
                deaths, droppedPreJoin, visible.size(),
                world.streams.screened().size());
  }
}

} // namespace

int main() {
  std::printf("test_membership: H3 3c-ii membership/replenishment/closure "
              "gates\n");

  partA_joinPrimitive();

  const auto pools = pltest::buildPoolSet(kSeed);
  partB_anchors(pools);
  partC_worldGates(pools);

  if (g_failures != 0) {
    std::fprintf(stderr, "test_membership: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("All membership gates passed.\n");
  return 0;
}
