//
// tests/test_card_endpoint_graph.cpp
//
// THE ENDPOINT LAYER MUST CARRY A MESSAGE (attacker-infra-2026-07).
//
// ===================================================================
// THE DEFECT THIS GATE EXISTS FOR, AND WHY NOTHING CAUGHT IT
//
// `buildCompromisePlans` minted a brand-new device and a brand-new IP
// for every unauthorized compromise:
//
//     .device = Identity{ring, 0xACE00000 + seq, 0}
//     .ip     = network::randomIpv4(rng)
//
// `seq` advanced once per accepted plan and the address came out of a
// ~3.5e9 space, so NO ATTACKER ENDPOINT WAS EVER SEEN TWICE. Cross-
// victim endpoint sharing was ZERO BY CONSTRUCTION. "One device touching
// many cards" is the single most valuable signal a card-fraud graph
// carries — it is the reason to model this as a graph rather than a
// table — and it was absent while four separate gates stayed green,
// because every one of them checked that endpoints were PRESENT and
// none checked that they were SHARED.
//
// That is the standing lesson this file encodes: A COUNT OF ENDPOINTS IS
// NOT A MEASUREMENT OF THE GRAPH. Degree distribution is the thing a
// message-passing model consumes, so degree distribution is what has to
// be gated.
//
// ===================================================================
// WHAT IS GATED
//
// A. REUSE EXISTS AND IS HEAVY-TAILED. Attacker endpoints must be seen
//    by more than one victim, the mean fan-out must clear a floor, and
//    the maximum must clear a much higher one — a flat degree
//    distribution would satisfy a mean-only gate while carrying none of
//    the structure an alert actually fires on.
//
// B. THE OPERATING POSITION SPLIT IS IN BAND. An unauthorized case is
//    operated from one of FIVE positions, and all five are counted
//    separately here: a persistent CAMPAIGN device, a single-use BURNER
//    fingerprint, an unrelated customer's BORROWED machine, a public
//    TERMINAL, or the VICTIM's own endpoint (remote-access / household
//    compromise). The classifier is `operatingPosition`; read its note
//    before touching the band, because three of those positions are NOT
//    attacker-owned and an `ownsDevice` test alone silently folds all
//    three into the victim bucket.
//
//    The realized victim share also ABSORBS every case the planner could
//    not attribute — no campaign live at the case date, no endpoint
//    covering the case span, no live host for the borrowed branch — so
//    this sub-gate is simultaneously the coverage check on the operator
//    pool's own sizing.
//
//    THE RESIDUAL IS NOT SEPARABLE FROM THE VICTIM BRANCH, and saying so
//    is the honest form of this gate: a row that kept the victim's
//    session carries no record of WHICH branch put it there. The band is
//    therefore on that TOTAL, anchored on the nominal share the planner
//    declares, with the headroom above it as the attribution budget. The
//    nominal is printed beside the realized value so the residual is
//    always visible even though it cannot be attributed.
//
//    Sub-gate B' gates the pool sizing DIRECTLY instead of inferring it:
//    mean concurrent campaigns, measured from the campaign spans. The
//    sizing rule claims that quantity is window-independent, and the two
//    legs — 4 years and 2 years, with the coverage floor binding in
//    both — are what test the claim rather than assume it.
//
// C. THE OWNERSHIP SHORTCUT IS DEAD, AND SIZED. `Has_Device`/`Has_IP`
//    shipped header-only for four rounds because "endpoint has no Party
//    edge" was an exact synonym for "attacker endpoint". This sub-gate
//    measures the rule's PRECISION on the card view and fails if it
//    climbs back toward determinism — while also requiring the feature
//    to retain real LIFT, because a shortcut replaced by pure noise
//    would be the opposite mistake. SIZE a shortcut before fixing it,
//    then keep sizing it.
//
// D. ATTACKER SESSIONS ARE POINT-IN-TIME HONEST. HARD ZERO: every row
//    carrying an attacker endpoint falls inside that endpoint's own
//    tenure. Not a band — the planner requires whole-case-span coverage
//    when it resolves, so an outside-tenure row is a logic error, and a
//    band would license the `device-ip-lifecycle` defect to return in
//    miniature.
//
// E. THE RESIDENTIAL-PROXY MECHANISM IS PRESENT. Attacker device beside
//    a CUSTOMER address on the same row. It is the structure that lets a
//    fraud transaction be reached from an unrelated Party through a
//    shared address node, and it is half of what kills C.
//
// F. THE SOCIAL-ENGINEERING RAILS HAVE NO ATTACKER ENDPOINT. HARD ZERO. A
//    scammer on the telephone produces no session: on the gift-card and
//    impostor-push rails the VICTIM operates the instrument, so the row
//    carries the victim's own routed endpoint and there is no attacker
//    device or address to attribute. Two independent guards enforce it
//    (`!scamRail` in the planner, `authorizedRail` in the typology), and
//    this sub-gate exists because both were previously covered only by a
//    hand-built unit fixture — which cannot catch a planner that starts
//    handing these rails an operator at production draw counts.
//
// I. DEGREE AND LABEL ARE NOT THE SAME COLUMN. Sub-gate A measures the
//    attacker side's degree and nothing else, so it cannot see the defect
//    that motivated this sub-gate: a person could present AT MOST TWO
//    card-view instruments, every legitimate device belonged to exactly one
//    person, and attacker devices served only compromise rows — so no
//    legitimate device could reach fan-out 3 and everything above it was
//    100% fraud. "device has >= 5 distinct cards => fraud" scored PRECISION
//    1.0000 at 33-36% recall, and A stayed green throughout because raising
//    the legitimate side moves none of its five numbers.
//
//    This sub-gate measures the JOINT distribution of degree and label:
//    where the all-fraud tail starts, the best precision any single
//    threshold can reach, the dispersion of the degree distribution, the
//    legitimate side's own reach, and how much fraud sits on an endpoint
//    that also carries legitimate traffic. It runs on the DEVICE and the IP
//    axis, on BOTH the exported card key and the generation-collapsed
//    account key, and it carries a temporal companion — because a shortcut
//    that is only closed on one of those is a shortcut relocated.
//
//    ITS COST IS MEASURED, NOT ASSUMED (`loc-accrual-perf-2026-08` rule 1).
//    It needs no exporter — it reads the posted ledger and reconstructs the
//    view filter — so it adds no run. Marginal cost against the same build
//    with the rollup writes compiled out, four legs: the shared row loop
//    goes 0.217 -> 0.614, 0.244 -> 0.713, 0.226 -> 0.644 and 0.239 -> 0.718
//    seconds, so +0.40 to +0.48 s per leg over 1.54-1.65M transactions, and
//    the sweep itself is 0.0009-0.0013 s. Against ~3.6 s of leg that is
//    ~12% of gate wall time. The cost is the two ordered maps of
//    `std::set`, one per endpoint axis, and it is linear in rows.
//
//    ITS LAST CHECK IS THE OPPOSITE DIRECTION AND IT IS NOT OPTIONAL. Cards
//    per person is now a distribution rather than a constant, and
//    `exposure.hpp` tilts unauthorized victim selection by ACTIVITY — so
//    more instruments means more activity means more victimisation, a live
//    path for card COUNT to become a learnable fraud signal. That lift is
//    banded to straddle 1.0, the same shape sub-gate G uses for the
//    merchant register.
//
// J. THE PUBLIC-ENDPOINT HEAD IS BOUNDED IN USERS, AT PRODUCTION
//    POPULATION. The legitimate population that closes I's cliff is a
//    pool of shared endpoints, and the first version of it bounded the
//    head on the drawn WEIGHT — a ratio, whose user count is
//    `peoplePerLine * w / E[w]` and therefore a hidden ceiling of about
//    1,008 nobody declared. It put a sixth to a quarter of every card
//    account in the world on ONE endpoint. Every other sub-gate here is
//    happy with that: those rows are legitimate, so degree-and-label
//    stays in band while the world says something false.
//
//    This sub-gate drives the SAMPLER at population 500,000 directly —
//    no ledger, milliseconds — because head share is emergent from
//    lines-per-roster and a 900-person leg cannot reach a production
//    value at any parameter setting. That is `merchant-selection` rule
//    8, and the gate-leg readings are PRINTED beside the banded ones.
//    Cards per endpoint is printed and banded NOWHERE: it is this law
//    times the cards-per-person law, and the second one moved in this
//    same round.
//
// ===================================================================
// WORLD SHAPE THIS GATE PINS (an equivalence gate must pin its world)
//
// The structural quantity in A is CASES PER OPERATOR, and it is a ratio
// of fraud volume to operator count. Operator count scales with
// population x campaign length; fraud volume scales with the corpus. A
// 900-person gate world at the production `targetTxnFraudP` would carry
// roughly one case per operator and part A would be measuring noise.
//
// So the legs RAISE THE FRAUD BUDGET to put cases-per-operator in the
// same range production runs at, and PRINT the realized ratio. That is a
// deliberate choice of which invariant to preserve: this gate is about
// STRUCTURE, not prevalence, and prevalence has its own suite
// (test_card_prevalence). Reproducing production's fraud RATE here would
// have meant reproducing none of its graph.
//
// TWO LEGS AT DIFFERENT SHAPES — a gate that only runs at one
// population is not coverage. Leg 1 is long and narrow (4 years, 900
// people), leg 2 short and wide (2 years, 1800). The sizing rule claims
// mean concurrent operators is population-driven and window-independent;
// two legs is the cheapest test of that claim.

#include "phantomledger/entities/holdings/card_reissue.hpp"
#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/derived_endpoints.hpp"
#include "phantomledger/entities/infra/enrollment.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/entities/infra/public_endpoints.hpp"
#include "phantomledger/entities/infra/random_ips.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices.hpp"
#include "phantomledger/synth/infra/ips.hpp"
#include "phantomledger/synth/infra/public_pool.hpp"
#include "phantomledger/synth/infra/tenure_table.hpp"
#include "phantomledger/synth/personas/join.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace channels = ::PhantomLedger::channels;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// ------------------------------------------------------------- bands
//
// Every edge below is a DECLARED CHOICE. The ones that can fail for a
// real reason are called out; the rest are tripwires against a mechanism
// disappearing.

// A. reuse
constexpr double kMinSharedDeviceShare = 0.35; // devices seen by >1 victim
constexpr double kMinMeanVictimsPerDevice = 2.0;
constexpr std::size_t kMinMaxVictimsPerDevice = 12; // the heavy tail exists
// Share of ATTACKER-OPERATED devices carrying at least one legitimate row.
// Derivation, disarm and the reason the set is not `ownsDevice` alone are at
// sub-gate C''.
constexpr double kMinOperatedMixedShare = 0.0511;
constexpr double kMinSharedIpShare = 0.25;
constexpr double kMinMeanVictimsPerIp = 1.5;

// B. operating position. The nominal victim-endpoint share is 0.30; the
// band's headroom above it is the attribution-failure budget (no live
// campaign, no endpoint covering the case span, or no live host device for
// the borrowed branch). If the realized value ever leaves this band the
// operator pool is mis-sized, which is exactly what this sub-gate is for.
//
// BOTH ENDS ARE MEASURED, NOT DERIVED, and the classifier they are measured
// against is the FIVE-WAY one — see `operatingPosition`. Four seed/shape pairs
// (900x1461, 1800x731, 1200x1096, 1500x900) read 0.3907 / 0.3506 / 0.3560 /
// 0.3614: mean 0.3647, sample SD 0.0179, mean + 3.5 SD = 0.4273.
//
// RE-MEASURED AFTER THE PUBLIC-TERMINAL BRANCH LANDED, not carried over from
// before it. The first sizing of this band read 0.3976 / 0.3568 / 0.3644 /
// 0.3678 against a construction with four operating positions; adding the
// fifth moved every leg. `merchant-selection-2026-08` rule 13 is exactly this
// case — a band measured against a superseded construction is not a
// measurement, even when the old numbers would still have passed.
//
// The FLOOR is the nominal itself rather than mean - 3.5 SD. The residual can
// only ADD to this share — an attribution failure falls into the victim bucket
// and nothing moves the other way — so a realized value below the declared
// share cannot be an attribution artifact and means the branch has stopped
// firing. Sizing the floor symmetrically would put it at 0.3090 and red a
// build whose attribution merely got better, which is the wrong direction to
// be brittle in.
constexpr double kVictimEndpointNominal = 0.30; // injector.cpp's declared share
constexpr double kVictimEndpointShareLo = 0.30;
constexpr double kVictimEndpointShareHi = 0.44;

// B'. mean CONCURRENT campaigns, the quantity the sizing rule's coverage
// floor is written in. The floor is 7.0; the band is what "the rule is
// working and window-independent" looks like from the outside, with room
// for the sampling spread of a few dozen lognormal campaign lengths.
//
// THIS SUB-GATE HAS ALREADY EARNED ITS KEEP. Its first run read 4.92 and
// 4.32 against a nominal 6.0 and turned red, and the cause was a real
// construction defect rather than a mis-set band: campaign starts were
// drawn over [0, W) instead of [-L, W), so the first ~L days of every run
// were covered only by campaigns beginning inside them. The disposition
// was to fix the generator, never to widen the band.
constexpr double kMeanConcurrentLo = 6.0;
constexpr double kMeanConcurrentHi = 14.0;

// C. the shortcut ceiling. Precision of "this endpoint has no Party
// association on file, therefore fraud", measured on the card view.
// Deterministic would be 1.0; this is the number that used to be 1.0 for
// the attacker half of the population.
constexpr double kMaxNotOnFilePrecision = 0.10;

// C'''. the same predicate read PER VERTEX, which is the form the exported
// graph presents and the form a model asks: one `cf_Device` row either has a
// `cf_Has_Device` edge or does not.
//
// MEASURED on the shipping build over the same four seed/shape pairs:
// precision 0.2827 / 0.2227 / 0.2598 / 0.2742, mean 0.2599, sample SD 0.0265,
// mean + 3.5 SD = 0.3526. Lift against the VERTEX base rate 1.56 / 1.47 /
// 1.57 / 1.60.
//
// THE CEILING IS AT mean + 3.5 SD RATHER THAN HELD SLACK, unlike sub-gate I's
// two ceilings, and the reason is the instrument: this is a plain proportion
// over ~1,100-1,400 vertices, not an ARGMAX taken over a sub-1%-recall tail,
// so it has no luckiest-draw term to track.
//
// NON-VACUOUS BY A LARGE MARGIN. The ownerless population is dominated by
// LEGITIMATE devices — public terminals, which can never hold an ownership
// edge, plus the ~28% of consumer machines the enrollment registry does not
// cover — and that is the only reason the precision sits at 0.26 rather than
// near 1.0. Remove the legitimate classes and the ownerless set becomes
// attacker campaign devices and burners, both ~100% fraud-carrying, which
// lands ~28 SD outside this ceiling.
//
// READ IT BESIDE C, NOT INSTEAD OF IT. The row form is the weaker rule here
// (precision 0.0255-0.0277 at 2.70-2.86x against the row base) because every
// ownerless class is low-row-count while consumer devices carry hundreds of
// rows each; the per-vertex form removes that weighting. Neither subsumes the
// other and both ship.
constexpr double kMaxOwnerlessVertexPrecision = 0.3526;

// E. residential proxy
constexpr double kMinProxyShare = 0.15;
constexpr double kMaxProxyShare = 0.45;

// I. degree and label.
//
// THE PREVALENCE CAVEAT COMES FIRST, because every band below is read at
// this leg's RAISED fraud budget and none of them would be readable at
// production prevalence. At `targetTxnFraudP` production and pop 900 x
// 1461d the defect this sub-gate exists for is INVISIBLE: max fan-out 6, no
// all-fraud tail, and the best single-threshold rule scores 0.0015 —
// exactly the base rate, i.e. no shortcut exists to measure. The raised
// budget is not a convenience, it is what gives this sub-gate power, for
// the same reason the file comment already gives for cases-per-operator.
// Read these numbers as statements about STRUCTURE at a fixed prevalence,
// never as production fraud rates.
//
// EVERY BAND BELOW IS RE-MEASURED ON THE SHIPPING BUILD over the same four
// seed/shape pairs the legs run (900x1461, 1800x731, 1200x1096, 1500x900),
// and the FLOORS are set at mean - 3.5 sample SD — the method
// `card-churn-2026-07` rule 3 established after a naive binomial came out
// 2.4x too tight, with the (n-1) divisor `kMinOperatedMixedShare` above
// already uses. The four readings are recorded beside each constant so a
// later round can tell a moved world from a mis-set band.
//
// AN EARLIER PASS OF THIS ROUND RECORDED READINGS THAT THE TREE NO LONGER
// PRODUCES, and correcting them is half of what this constant block is for.
// It claimed firstAllFraudK 73/61/68/66 against a realized 31/33/31/33, and
// best account-key precision 0.0966-0.1247 against a realized 0.0271-0.0582
// — the public-terminal recalibration landed between the two. The bands were
// still satisfied, which is exactly why this was invisible:
// `merchant-selection-2026-08` rule 13 says a band measured against a
// superseded construction is not a measurement, and a RECORDED READING goes
// stale the same way. Numbers below are from the build in this commit.
//
// THE TWO CEILINGS ARE DELIBERATELY NOT AT mean + 3.5 SD, and the reason is
// in the instrument rather than in the world. `degreeLabelSweep` reports an
// ARGMAX, and on two of the four legs that argmax sits at k = 331 / 399 —
// the single widest endpoint, recall 0.0083 / 0.0098. A ceiling pinned near
// a maximum taken over a sub-1%-recall tail tracks that tail's luckiest
// draw. Both ceilings are held at the value that catches the failure they
// exist for (precision -> 1.0; the disarm below reproduces 1.0000 exactly)
// and the tight figure is recorded beside them so the slack is visible.
constexpr std::size_t kMinFirstAllFraudK = 20;
constexpr double kMaxDegreePrecision = 0.30;
constexpr double kMinDegreeFano = 11.40;
constexpr std::size_t kMinLegitP99Degree = 22;
constexpr std::size_t kMinConsumerP99Degree = 5;
constexpr double kMinMixedDeviceShare = 0.4944;
constexpr double kMinMixedIpShare = 0.5939;
constexpr double kMaxBurstPrecision = 0.25;
constexpr double kCardCountLiftLo = 0.80;
constexpr double kCardCountLiftHi = 1.25;

// I.9 on the CLEAN world variable. A bucket thinner than this is printed but
// not banded: the cited Fed count law's tail is genuinely sparse, and a
// 35-person bucket's own standard error is wide enough that banding it
// asserts nothing. MEASURED at the four legs, this bands 0-4 cards on
// leg-long, 0-8 on leg-wide and leg-sizeB, and 0-6 on leg-sizeA — i.e. the
// whole distribution wherever the leg is large enough to populate it, and the
// sparse tail is printed.
constexpr std::size_t kMinHeldBucketPeople = 60;

// The ACTIVITY ceiling: card-view rows per person for cardholders with 2 or
// more cards, over the same for cardholders with exactly one.
//
// MEASURED on the shipping build over the four legs: 1.0623 / 1.0689 /
// 1.0976 / 1.0851, mean 1.0785, sample SD 0.0160, mean + 3.5 SD = 1.134.
//
// WHAT IT GUARDS, AND ITS HONEST LIMIT. The coupling is real, systematic and
// small, and its cause is the one quantity in the model that scales linearly
// with card count: every card draws its own `persona.card.limit`, so K cards
// is K times the revolving headroom, more rows ride credit, the deposit pool
// drains more slowly and the liquidity multiplier emits more. It saturates
// exactly at `actors::kMaxCreditInstruments`, which is the tell that the
// spend-active count is the driver.
//
// It is BOUNDED rather than removed on purpose. More cards meaning more
// spend is realistic; forcing it to exactly 1.0 would be
// `merchant-selection-2026-08` rule 1 in a new costume, and the label side
// is flat, so nothing is leaking today. What would break it is a change that
// let per-card resources reach the emission rate harder — raising
// `kMaxCreditInstruments` from 4 to 8 is the concrete one, since the curve
// currently flattens at the cap.
//
// NO DISARM DRIVES THIS RED TODAY, AND THAT IS STATED RATHER THAN IMPLIED
// AWAY. Every switch in this file moves the endpoint layer, not the
// instrument layer, so this ceiling ships as a forward guard whose failure
// mode is named but not demonstrated — the weaker of the two postures this
// file uses, and the reason the label check beside it carries the disarm.
constexpr double kMaxHeldActivityRatio = 1.134;

// The POOLED label ratio: mean fraud rows per person for cardholders holding
// 4 or more cards, over the same for cardholders holding 1-3. This is the
// high-power form of I.9's label question.
//
// MEASURED: 0.951 / 0.980 / 0.980 / 1.003, mean 0.9785, sample SD 0.0217,
// mean +/- 3.5 SD = [0.9025, 1.0545]. Rounded outward to [0.90, 1.06].
//
// IT SITS SLIGHTLY BELOW 1.0 ON EVERY LEG, and that is the correct sign
// rather than a miss: nothing in victim selection reads card count, so the
// null is exact, and the small deficit is the same overdispersion the
// per-bucket errors report.
constexpr double kHeldHalfRatioLo = 0.90;
constexpr double kHeldHalfRatioHi = 1.06;

/* SUB-GATE G' — the largest owned-share gap between fraud-touched merchants
 * and view merchants overall that a clean register produces. Sized BETWEEN the
 * armed maximum (0.0487) and the weight-disarm minimum (0.1072); see the
 * derivation at the check itself. */
constexpr double kMaxOwnedShareGap = 0.08;

/* SUB-GATE G'' — online-to-physical owner coverage ratio. Armed 1.0632 /
 * 0.9268 / 1.2756 / 0.8104, mean 1.0190, sample SD 0.2000, mean +/- 3.5 SD.
 * Footprint disarm reads 0.0000 on every leg. */
constexpr double kModalityRatioLo = 0.32;
constexpr double kModalityRatioHi = 1.72;

struct Leg {
  const char *name;
  std::uint64_t seed;
  int startYear;
  std::int32_t days;
  std::int32_t population;
  double targetTxnFraudP;
};

[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  const pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

[[nodiscard]] pl::pipeline::SimulationResult
runLeg(const pl::synth::pii::PoolSet &poolSet, const Leg &leg) {
  const pl::time::Window window{
      .start = pl::time::makeTime({leg.startYear, 1, 1}),
      .days = leg.days,
  };

  // Ring rate raised so the world carries AML rings at this population
  // (the ring count rounds to zero below ~833 people), and the fraud
  // budget raised for the cases-per-operator reason in the file comment.
  // Neither touches the mechanism under test.
  pl::synth::people::Fraud fraudProfile{};
  fraudProfile.rings.perTenKMean = 20.0;
  fraudProfile.rings.perTenKSigma = 0.0;
  fraudProfile.limits.targetTxnFraudP = leg.targetTxnFraudP;

  const pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = leg.population,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

  pl::clearing::BalanceRules balanceRules{};
  pl::transfers::credit_cards::LifecycleRules lifecycleRules{};

  auto rng = pl::random::Rng::fromSeed(leg.seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window, entities, leg.seed};
  pipeline.transferStage()
      .legit()
      .window(window)
      .seed(leg.seed)
      .openingBalanceRules(&balanceRules)
      .creditLifecycle(&lifecycleRules);
  pipeline.transferStage().fraud().profile(&fraudProfile);

  return pipeline.run();
}

struct Fanout {
  std::size_t endpoints = 0;
  std::size_t shared = 0;
  std::size_t maxVictims = 0;
  double meanVictims = 0.0;
};

template <typename Key>
[[nodiscard]] Fanout
summarize(const std::map<Key, std::set<pl::entity::Key>> &m) {
  Fanout out;
  out.endpoints = m.size();
  std::size_t total = 0;
  for (const auto &[key, victims] : m) {
    total += victims.size();
    out.maxVictims = std::max(out.maxVictims, victims.size());
    if (victims.size() > 1) {
      ++out.shared;
    }
  }
  out.meanVictims = out.endpoints == 0 ? 0.0
                                       : static_cast<double>(total) /
                                             static_cast<double>(out.endpoints);
  return out;
}

/* THE FIVE OPERATING POSITIONS AN UNAUTHORIZED ROW CAN SIT IN.
 *
 * This used to be a two-way split — attacker-owned device, or "everything
 * else", the latter read as the victim's own endpoint. Three positions have
 * since been added between those poles and NONE is attacker-OWNED:
 *
 *   * a BURNER is exogenous attacker infrastructure that is deliberately kept
 *     out of `AttackerInfra::sortedDevices`, so `ownsDevice` is false for it by
 *     design — that is what keeps a single-use fingerprint from dragging
 *     sub-gate A's reuse means down;
 *   * a BORROWED device belongs to a real, unrelated customer;
 *   * a TERMINAL is a public machine, owned by nobody in the roster.
 *
 * Under the old classifier all three landed in the victim bucket, which took
 * the measured share from ~0.20 to ~0.55 and reds a band that is still correct
 * for the quantity it names. `merchant-selection-2026-08` rule 13: A BAND
 * MEASURED AGAINST A SUPERSEDED CONSTRUCTION IS NOT A MEASUREMENT — so the
 * INSTRUMENT is corrected here rather than the band widened to absorb positions
 * it cannot see. The four attacker-operated positions are counted separately
 * and the band stays on the victim share alone, which is what it was always
 * about. */
enum class Position { campaign, burner, borrowed, terminal, victim };

[[nodiscard]] Position
operatingPosition(const pl::transactions::Transaction &tx,
                  const pl::infra::AttackerInfra &attackers,
                  const pl::infra::Router &router) {
  const auto id = tx.session.deviceId;
  if (attackers.ownsDevice(id)) {
    return Position::campaign;
  }
  /* A burner keeps the campaign's ownerType and ownerId so internal
   * attribution survives; only the SLOT is lifted above the mint's range. */
  if (id.ownerType == pl::devices::OwnerType::ring &&
      id.ownerId >= pl::infra::kAttackerOwnerIdBase &&
      id.slot >= pl::infra::derived::kBurnerSlotBase) {
    return Position::burner;
  }
  if (id.ownerType == pl::devices::OwnerType::publicTerminal) {
    return Position::terminal;
  }
  /* A consumer device is the VICTIM's only if it belongs to the person who
   * owns the compromised account. Anyone else's is a borrowed machine. */
  if (id.ownerType == pl::devices::OwnerType::person) {
    const auto victim = router.ownerOf(tx.source);
    if (victim.has_value() &&
        id.ownerId != static_cast<std::uint64_t>(*victim)) {
      return Position::borrowed;
    }
  }
  return Position::victim;
}

/* SUB-GATE I's per-endpoint rollup. Both keys are carried because they
 * differ by more than an order of magnitude in the low buckets and the
 * round they were built for could have been passed by moving only one:
 * `accounts` is the generation-collapsed world quantity, `cards` is what
 * the exporter writes, since `derive::cardId` appends `-G<n>` and a
 * reissued card is the same instrument wearing a new number. */
struct EndpointRollup {
  std::set<pl::entity::Key> accounts;
  std::set<std::pair<pl::entity::Key, std::uint32_t>> cards;
  std::set<std::int32_t> activeDays;
  std::size_t rows = 0;
  std::size_t fraudRows = 0;
  bool ringOwned = false;
  /* A CONSUMER device is `OwnerType::person`: one real roster member's own
   * machine. It is carried separately from `ringOwned` because "not an
   * attacker device" is a much larger set than "a person's device" — it
   * also holds the public terminals and the shared household lines, whose
   * reach is enormous and has nothing to say about the per-person
   * instrument ceiling this round exists to remove. */
  bool personOwned = false;
  /* Did an ATTACKER actually operate this endpoint? True for a consumer
   * machine only when it carried at least one row in the `borrowed`
   * position — an attacker's row against SOMEONE ELSE.
   *
   * IT IS NOT "THIS DEVICE BELONGS TO A COMPROMISED HOST", and the
   * difference is what sub-gate C'' was getting wrong. A host owns 1-6
   * devices across a replacement chain and the attacker uses one of them
   * for one case; the others carry only their owner's ordinary traffic, so
   * they score `fraudRows == 0 < rows` — MIXED — without an attacker ever
   * having touched them. Counting them made C'''s numerator the count of
   * host-owned devices in the view rather than a measurement of mixing.
   *
   * Nor is it "carried a fraud row": the victim-endpoint branch puts the
   * OWNER's OWN compromise on their own machine, which is a real mixed
   * endpoint but not an attacker-OPERATED one. `operatingPosition` already
   * separates the two and this just remembers its verdict. */
  bool attackerOperated = false;
};

struct DegreePoint {
  std::size_t degree = 0;
  std::size_t rows = 0;
  std::size_t fraudRows = 0;
};

struct DegreeReport {
  std::size_t endpoints = 0;
  std::size_t maxDegree = 0;
  double meanDegree = 0.0;
  double fano = 0.0;
  /* The smallest fan-out at which EVERY endpoint is pure fraud. Zero means
   * no such bucket exists, which is the healthy reading. */
  std::size_t firstAllFraudK = 0;
  std::size_t bestK = 0;
  double bestPrecision = 0.0;
  double bestRecall = 0.0;
  std::size_t fraudAtDegreeOne = 0;
};

/* The joint degree/label sweep. Precision is over ROWS, not endpoints,
 * because a threshold rule is applied to transactions; recall is against
 * the whole card view's fraud so the two legs' rules are comparable.
 *
 * Ties on precision resolve to the HIGHEST k, which reports the tightest
 * form of the rule rather than the widest — the pessimistic reading.
 *
 * THE ARGMAX SKIPS k = 1, AND THAT IS A CORRECTION, NOT A CONVENIENCE.
 * `fan-out >= 1` is satisfied by every endpoint in the view, so it scores
 * exactly the base rate at recall 1.0 and is not a threshold rule at all.
 * The first version of this sweep included it, and on the IP axis it WON on
 * all four legs — reporting "best rule k>=1, precision 0.0092-0.0097, lift
 * 1.00x", which reads like a measured null and is really the sweep saying it
 * found nothing. Excluding it makes the IP reading a statement about a rule
 * a model could actually learn, and it moves no device-axis number (the
 * device argmax sits at k = 37-399 on every leg). */
[[nodiscard]] DegreeReport degreeLabelSweep(const std::vector<DegreePoint> &pts,
                                            std::size_t viewFraudRows) {
  DegreeReport out;
  out.endpoints = pts.size();
  if (pts.empty()) {
    return out;
  }
  double total = 0.0;
  for (const auto &p : pts) {
    total += static_cast<double>(p.degree);
    out.maxDegree = std::max(out.maxDegree, p.degree);
  }
  out.meanDegree = total / static_cast<double>(pts.size());
  double squared = 0.0;
  for (const auto &p : pts) {
    const auto d = static_cast<double>(p.degree) - out.meanDegree;
    squared += d * d;
  }
  const double variance = squared / static_cast<double>(pts.size());
  out.fano = out.meanDegree > 0.0 ? variance / out.meanDegree : 0.0;

  std::map<std::size_t, std::size_t> bucketRows;
  std::map<std::size_t, std::size_t> bucketFraud;
  std::map<std::size_t, std::size_t> bucketClean;
  for (const auto &p : pts) {
    bucketRows[p.degree] += p.rows;
    bucketFraud[p.degree] += p.fraudRows;
    if (p.fraudRows == 0) {
      ++bucketClean[p.degree];
    }
  }
  if (const auto it = bucketFraud.find(1); it != bucketFraud.end()) {
    out.fraudAtDegreeOne = it->second;
  }
  for (const auto &[k, rows] : bucketRows) {
    if (rows > 0 && bucketClean[k] == 0) {
      out.firstAllFraudK = k;
      break;
    }
  }

  std::size_t cumRows = 0;
  std::size_t cumFraud = 0;
  for (auto it = bucketRows.rbegin(); it != bucketRows.rend(); ++it) {
    cumRows += it->second;
    cumFraud += bucketFraud[it->first];
    if (cumRows == 0 || it->first <= 1) {
      continue;
    }
    const double precision =
        static_cast<double>(cumFraud) / static_cast<double>(cumRows);
    if (precision > out.bestPrecision) {
      out.bestPrecision = precision;
      out.bestK = it->first;
      out.bestRecall = viewFraudRows == 0
                           ? 0.0
                           : static_cast<double>(cumFraud) /
                                 static_cast<double>(viewFraudRows);
    }
  }
  return out;
}

/* Nearest-rank percentile over an unweighted endpoint population. */
[[nodiscard]] std::size_t percentileDegree(std::vector<std::size_t> degrees,
                                           double q) {
  if (degrees.empty()) {
    return 0;
  }
  std::ranges::sort(degrees);
  const auto rank = static_cast<std::size_t>(
      std::ceil(q * static_cast<double>(degrees.size())));
  const auto index = rank == 0 ? 0 : rank - 1;
  return degrees[std::min(index, degrees.size() - 1)];
}

void measure(const Leg &leg, const pl::pipeline::SimulationResult &result) {
  const auto &attackers = result.infra.attackers;
  const auto &router = result.infra.router;
  const auto &txns = result.transfers.ledger.posted.txns;

  std::printf("\n=== %s: pop %d, %d days, seed %llu ===\n", leg.name,
              leg.population, leg.days,
              static_cast<unsigned long long>(leg.seed));
  std::printf("  operators %zu, attacker devices %zu, attacker IPs %zu\n",
              attackers.operatorCount(), attackers.deviceCount(),
              attackers.ipCount());

  check(attackers.operatorCount() > 0,
        std::string(leg.name) + ": the world must carry attacker operators; "
                                "with none, every sub-gate below is vacuous");

  // ------------------------------------------- endpoint -> tenure index
  std::map<pl::devices::Identity, std::vector<pl::infra::Tenure>> deviceSpans;
  for (std::size_t i = 0; i < attackers.devices.size(); ++i) {
    deviceSpans[attackers.devices[i]].push_back(attackers.deviceTenures[i]);
  }
  std::map<pl::network::Ipv4, std::vector<pl::infra::Tenure>> ipSpans;
  for (std::size_t i = 0; i < attackers.ips.size(); ++i) {
    ipSpans[attackers.ips[i]].push_back(attackers.ipTenures[i]);
  }

  // ------------------------------- the institution's endpoint registry
  // A device has a `Has_Device` row iff SOME enrolled usage names it,
  // which is exactly the condition the exporter writes on.
  std::unordered_set<pl::devices::Identity> deviceOnFile;
  for (const auto &u : result.infra.devices.usages) {
    if (u.enrolled) {
      deviceOnFile.insert(u.deviceId);
    }
  }
  std::unordered_set<pl::network::Ipv4> ipOnFile;
  for (const auto &u : result.infra.ips.usages) {
    if (u.enrolled) {
      ipOnFile.insert(u.ipAddress);
    }
  }
  // Customer-held endpoints regardless of registry coverage — needed to
  // separate "a customer's address" from "attacker infrastructure" in
  // the residential-proxy count.
  std::unordered_set<pl::network::Ipv4> customerIp;
  for (const auto &u : result.infra.ips.usages) {
    customerIp.insert(u.ipAddress);
  }

  static constexpr auto kCardTag = channels::tag(channels::Legit::cardPurchase);
  static constexpr auto kMerchantTag = channels::tag(channels::Legit::merchant);

  std::map<pl::devices::Identity, std::set<pl::entity::Key>> victimsPerDevice;
  std::map<pl::network::Ipv4, std::set<pl::entity::Key>> victimsPerIp;

  std::size_t unauthorizedRows = 0;
  std::size_t unauthorizedOnAttackerDevice = 0;
  std::size_t unauthorizedOnVictimDevice = 0;
  std::size_t unauthorizedOnBurnerDevice = 0;
  std::size_t unauthorizedOnBorrowedDevice = 0;
  std::size_t unauthorizedOnTerminal = 0;
  std::size_t attackerRowsOutsideDeviceTenure = 0;
  std::size_t attackerRowsOutsideIpTenure = 0;
  std::size_t proxyRows = 0;
  std::size_t operatorInfraRows = 0;
  std::size_t scamRailRows = 0;
  std::size_t scamRailWithAttackerEndpoint = 0;
  std::size_t scamRailSessionless = 0;
  std::size_t giftCardRows = 0;
  std::size_t giftCardOnlineRows = 0;

  std::size_t viewOwnedMerchant = 0;
  std::size_t viewOwnedMerchantFraud = 0;

  /* THE MERCHANT-LEVEL FORM of the same question, and the one with the
   * power. See sub-gate G' for why the row-weighted pair above cannot
   * answer it. */
  std::set<pl::entity::Key> viewMerchantSet;
  std::set<pl::entity::Key> viewFraudMerchantSet;

  // Sub-gate H. Keyed on the card, memoised, and computed over the SAME
  // window bounds the exporter's `generationFor` uses — a mismatch here would
  // measure a different schedule than the one that reached the tables.
  std::size_t viewMultiGenCard = 0;
  std::size_t viewMultiGenCardFraud = 0;
  std::map<pl::entity::Key, bool> multiGenerationCard;
  // DISARM SWITCH for sub-gate H, left in place because the check's whole
  // claim is a NEGATIVE, and a negative that has never been made to fail is
  // indistinguishable from a check that cannot fail. Flipping this to true
  // simulates fraud-driven reissue — the real-world mechanism deliberately
  // omitted from `card_reissue.hpp` — by marking every compromised card as
  // reissued. MEASURED with it on: lift 1.153x (leg-long) and 1.478x
  // (leg-wide); both clear the ceiling below, so both legs red.
  //
  // THE ASYMMETRY BETWEEN THE LEGS IS THE INTERESTING PART. Leg-long is a
  // 4-year window where 66% of view cards already reissue, so forcing the
  // compromised ones true adds little contrast — the flag has SATURATED, and
  // a saturated flag is weak evidence of anything. Leg-wide's 2-year window
  // sits at 43% and the same leak shows three times as strongly. Sensitivity
  // here therefore DEGRADES with window length, which is worth knowing before
  // trusting this sub-gate on a 20-year corpus.
  constexpr bool kDisarmFraudDrivenReissue = false;

  /* I.9's disarm: a synthetic per-card fraud tilt. Zero ships. See the use
   * site inside I.9 for why it has to be an INSTRUMENT disarm and for the
   * power figure it measures. */
  constexpr double kDisarmInstrumentExposure = 0.0;
  std::set<pl::entity::Key> fraudCards;
  if (kDisarmFraudDrivenReissue) {
    for (const auto &tx : txns) {
      if (tx.fraud.flag != 0) {
        fraudCards.insert(tx.source);
      }
    }
  }
  const auto legStart = pl::time::makeTime({leg.startYear, 1, 1});
  const auto legStartEpoch = pl::time::toEpochSeconds(legStart);
  const auto legEndEpoch =
      pl::time::toEpochSeconds(pl::time::addDays(legStart, leg.days));

  // Destination -> acceptance footprint, from the world's OWN catalogue.
  std::map<pl::entity::Key, pl::entity::merchant::Footprint> footprintByKey;
  std::map<pl::entity::Key, pl::entity::PersonId> ownerByMerchant;
  for (const auto &rec : result.counterparties.merchants.records) {
    footprintByKey.emplace(rec.counterpartyId, rec.footprint);
    if (rec.owner != pl::entity::invalidPerson) {
      ownerByMerchant.emplace(rec.counterpartyId, rec.owner);
    }
  }

  /* DISARM FOR SUB-GATE G', and it drives the exact leak the header warns
   * about: "small local outlets have proprietors, national services and
   * online merchants do not". That rule is the TEMPTING version of this
   * feature and it would be a label shortcut, because the card rail is
   * heavily card-not-present and draws only from `Footprint::online`, so any
   * eligibility rule reading `footprint` inherits the modality split.
   *
   * Left in the file permanently. A negative that has never been made to fail
   * is indistinguishable from a check that cannot. */
  if (const char *mode = std::getenv("PL_OWNERSHIP_DISARM")) {
    const std::string which{mode};
    ownerByMerchant.clear();
    if (which == "weight") {
      /* THE OTHER ATTRIBUTE CLAUDE.md RULE 1 NAMES. A register keyed on
       * popularity does not touch footprint at all, so G'' cannot see it —
       * but merchants are selected for fraud by weight, so it lands straight
       * on the destination side of the label. This is G's disarm. */
      std::vector<double> weights;
      weights.reserve(result.counterparties.merchants.records.size());
      for (const auto &rec : result.counterparties.merchants.records) {
        weights.push_back(rec.weight);
      }
      auto sorted = weights;
      std::sort(sorted.begin(), sorted.end());
      const double cut =
          sorted.empty()
              ? 0.0
              : sorted[static_cast<std::size_t>(
                    static_cast<double>(sorted.size()) *
                    (1.0 - pl::entity::merchant::ownership::
                               kBeneficialOwnerCoverage))];
      for (const auto &rec : result.counterparties.merchants.records) {
        if (rec.weight >= cut) {
          ownerByMerchant.emplace(rec.counterpartyId,
                                  static_cast<pl::entity::PersonId>(1));
        }
      }
    } else {
      /* Default: the footprint leak. */
      for (const auto &rec : result.counterparties.merchants.records) {
        if (rec.footprint != pl::entity::merchant::Footprint::online) {
          ownerByMerchant.emplace(rec.counterpartyId,
                                  static_cast<pl::entity::PersonId>(1));
        }
      }
    }
  }

  /* THE CARD VIEW IS THE CHANNEL FILTER **AND** THE MEMBERSHIP CLAUSE.
   *
   * `StreamingCardFraudExport::append` drops a row whose source owner or
   * target owner is not a member at the row's own timestamp, and this loop
   * used to apply only the channel tag. Every card-view number in this file
   * was therefore computed over a SUPERSET of the exported table — harmless
   * for a share, but not for a degree: an endpoint's fan-out is a count of
   * distinct partners, so rows the table does not contain can only inflate
   * it, and sub-gate I would have banded a distribution nobody can see.
   *
   * Reconstructed through `join_cohort::membershipOf`, which the exporter's
   * own comment names as THE one construction path, and through the same
   * `ownerOf` resolution order — credit-card registry first, then the
   * account registry — so the two filters agree by construction rather than
   * by coincidence. The drop is PRINTED, because an unmeasured gap between
   * a gate's view and the exporter's is exactly what this fixes. */
  const pl::time::Window legWindow{.start = legStart, .days = leg.days};
  const auto membership = pl::synth::personas::join_cohort::membershipOf(
      result.people.personas, legWindow);
  const auto &accountRegistry = result.holdings.accounts.registry;
  const auto &accountLookup = result.holdings.accounts.lookup;
  const auto &cardRegistry = result.holdings.creditCards;
  const auto ownerOfKey =
      [&](pl::entity::Key key) noexcept -> pl::entity::PersonId {
    if (const auto *card = cardRegistry.forKey(key)) {
      return card->owner;
    }
    const auto it = accountLookup.byId.find(key);
    if (it == accountLookup.byId.end() ||
        it->second >= accountRegistry.records.size()) {
      return pl::entity::invalidPerson;
    }
    return accountRegistry.records[it->second].owner;
  };
  std::size_t channelRows = 0;

  // Sub-gate I. One rollup per endpoint over the card view, both keys.
  std::map<pl::devices::Identity, EndpointRollup> deviceRollup;
  std::map<pl::network::Ipv4, EndpointRollup> ipRollup;
  std::map<pl::entity::PersonId, EndpointRollup> personRollup;
  std::map<pl::entity::Key, std::vector<pl::entity::card::reissue::Generation>>
      generationsByCard;

  // Card-view confusion counts for sub-gate C.
  std::size_t viewRows = 0;
  std::size_t viewFraud = 0;
  std::size_t viewDeviceNotOnFile = 0;
  std::size_t viewDeviceNotOnFileFraud = 0;
  std::size_t viewIpNotOnFile = 0;
  std::size_t viewIpNotOnFileFraud = 0;
  std::set<pl::devices::Identity> cardViewAttackerDevices;
  std::set<pl::network::Ipv4> cardViewAttackerIps;

  for (const auto &tx : txns) {
    const bool fraud = tx.fraud.flag != 0;
    const bool attackerDevice = attackers.ownsDevice(tx.session.deviceId);
    const bool attackerIp = attackers.ownsIp(tx.session.ipAddress);

    // The UNAUTHORIZED family only. The two authorized scam rails are
    // victim-operated by declaration (victim-session-2026-07) and would
    // dilute the operating-position split with rows that were never
    // eligible for an attacker endpoint.
    if (fraud && tx.fraud.type == pl::fraud::FraudType::txnFraudSolo) {
      ++unauthorizedRows;
      switch (operatingPosition(tx, attackers, router)) {
      case Position::campaign:
        ++unauthorizedOnAttackerDevice;
        break;
      case Position::burner:
        ++unauthorizedOnBurnerDevice;
        break;
      case Position::borrowed:
        ++unauthorizedOnBorrowedDevice;
        /* The only place a consumer machine is known to have been
         * attacker-OPERATED. Recorded on the rollup so sub-gate C'' can
         * count endpoints an attacker used rather than endpoints a
         * compromised host happens to own. */
        deviceRollup[tx.session.deviceId].attackerOperated = true;
        break;
      case Position::terminal:
        ++unauthorizedOnTerminal;
        break;
      case Position::victim:
        ++unauthorizedOnVictimDevice;
        break;
      }
    }

    // ------- F: THE SOCIAL-ENGINEERING RAILS HAVE NO ATTACKER ENDPOINT
    //
    // A scammer on the telephone does not produce a session. On the two
    // AUTHORIZED rails the person operating the payment instrument IS
    // THE VICTIM — walking into a store to buy gift cards, or logging
    // into their own banking app to push a transfer — so the row must
    // carry the victim's own routed device and address, and there is no
    // attacker endpoint to attribute in the first place.
    //
    // Asserting it HERE, on the corpus, is the point. The property was
    // gated only in `test_unauthorized_keyed`'s hand-built Run C
    // fixture, which cannot catch a PLANNER that starts handing these
    // rails an operator: `injector.cpp` guards on `!scamRail` and
    // `unauthorized.cpp` guards again on `authorizedRail`, and a
    // corpus-level zero is what proves BOTH guards are still doing their
    // job at production draw counts.
    if (fraud && (tx.fraud.type == pl::fraud::FraudType::scamGiftCard ||
                  tx.fraud.type == pl::fraud::FraudType::scamImpostor)) {
      ++scamRailRows;
      if (attackerDevice || attackerIp) {
        ++scamRailWithAttackerEndpoint;
      }
      if (!tx.session.deviceId.assigned()) {
        ++scamRailSessionless;
      }
    }

    // BOTH GIFT-CARD CHANNELS MUST EXIST (giftcard-channel-2026-07). The
    // rail was hardcoded card-PRESENT, so 100% of coached gift-card
    // purchases were in-store swipes and the ONLINE branch — the one where
    // an issuer has a live session to score and something to interrupt
    // before the codes are read out — did not exist in the corpus at all.
    //
    // Resolved against the leg's OWN acceptance catalogue, the same way
    // the exporter resolves `use_chip`, so this measures what the row will
    // actually render as rather than re-deriving the decision.
    if (fraud && tx.fraud.type == pl::fraud::FraudType::scamGiftCard) {
      ++giftCardRows;
      const auto it = footprintByKey.find(tx.target);
      if (it != footprintByKey.end() &&
          it->second == pl::entity::merchant::Footprint::online) {
        ++giftCardOnlineRows;
      }
    }

    if (attackerDevice) {
      victimsPerDevice[tx.session.deviceId].insert(tx.source);
      const auto it = deviceSpans.find(tx.session.deviceId);
      const bool inside =
          it != deviceSpans.end() &&
          std::ranges::any_of(it->second, [&](const pl::infra::Tenure &t) {
            return t.contains(tx.timestamp);
          });
      if (!inside) {
        ++attackerRowsOutsideDeviceTenure;
      }

      if (attackerIp) {
        ++operatorInfraRows;
      } else if (customerIp.contains(tx.session.ipAddress)) {
        ++proxyRows;
      }
    }

    if (attackerIp) {
      victimsPerIp[tx.session.ipAddress].insert(tx.source);
      const auto it = ipSpans.find(tx.session.ipAddress);
      const bool inside =
          it != ipSpans.end() &&
          std::ranges::any_of(it->second, [&](const pl::infra::Tenure &t) {
            return t.contains(tx.timestamp);
          });
      if (!inside) {
        ++attackerRowsOutsideIpTenure;
      }
    }

    const auto channel = tx.session.channel;
    if (channel != kCardTag && channel != kMerchantTag) {
      continue;
    }
    ++channelRows;
    if (!membership.activeAt(ownerOfKey(tx.source), tx.timestamp) ||
        !membership.activeAt(ownerOfKey(tx.target), tx.timestamp)) {
      continue;
    }
    ++viewRows;
    if (fraud) {
      ++viewFraud;
    }
    // THE LABELLING GAP (attacker-infra-2026-07). Attacker endpoints used
    // to reach the exporter with a hard-coded `flagged = false`, so the
    // only `device/flagged` positives in the whole overlay were the 5 AML
    // ring-shared devices — which enter the card view ONLY through a ring
    // member's LEGITIMATE purchase. Entity-level device ground truth was
    // both vanishing AND anti-correlated with the transaction label.
    // These counters are what make "the overlay is now non-degenerate" a
    // measurement rather than a claim.
    if (attackerDevice) {
      cardViewAttackerDevices.insert(tx.session.deviceId);
    }
    if (attackerIp) {
      cardViewAttackerIps.insert(tx.session.ipAddress);
    }
    if (!deviceOnFile.contains(tx.session.deviceId)) {
      ++viewDeviceNotOnFile;
      if (fraud) {
        ++viewDeviceNotOnFileFraud;
      }
    }
    if (!ipOnFile.contains(tx.session.ipAddress)) {
      ++viewIpNotOnFile;
      if (fraud) {
        ++viewIpNotOnFileFraud;
      }
    }

    // ------- G: THE MERCHANT OWNERSHIP REGISTER CARRIES NO FRAUD SIGNAL
    //
    // `cf_Is_Merchant` links a merchant to its proprietor Party, and
    // merchants are where card fraud LANDS — so if register membership
    // correlated with the label at all, the ownership edge would be a
    // shortcut into the destination side of every fraud row.
    //
    // It cannot, by construction: `ownership::onFile` hashes the merchant
    // KEY alone and reads no other attribute. This measures that the
    // construction actually holds end-to-end, because the tempting
    // version of this feature — small local outlets have proprietors,
    // national services and online merchants do not — WOULD have leaked:
    // the card rail is ~70% card-not-present and draws only from
    // `Footprint::online`, so any eligibility rule reading `footprint` or
    // `weight` inherits the modality split and with it the label.
    if (const auto it = ownerByMerchant.find(tx.target);
        it != ownerByMerchant.end()) {
      ++viewOwnedMerchant;
      if (fraud) {
        ++viewOwnedMerchantFraud;
      }
    }

    /* One entry per DESTINATION, not per row: the unit the ownership hash
     * actually decides. */
    viewMerchantSet.insert(tx.target);
    if (fraud) {
      viewFraudMerchantSet.insert(tx.target);
    }

    // ------- H: THE REISSUE SCHEDULE CARRIES NO FRAUD SIGNAL
    //
    // card-churn-2026-07 gives one `cf_Card` vertex per OBSERVED card
    // generation, so "this party holds more than one card number" became an
    // exported, model-visible feature. In the real world that feature
    // predicts fraud strongly and for the wrong reason: an issuer reissues
    // BECAUSE a card was compromised. Modelling that would make card
    // cardinality a label, so fraud-driven reissue is deliberately absent
    // (see card_reissue.hpp) — and this is the check that the absence holds
    // end-to-end rather than only in the header comment.
    //
    // WHY THE FLAG IS RULE-LEVEL, NOT OBSERVED. Counting generations that
    // actually appear in the view would confound the measurement with
    // EXPOSURE: a card with more rows is more likely to straddle a boundary
    // AND — via `exposure.hpp`, which tilts unauthorized victim selection by
    // card activity — more likely to be victimized. That correlation is real
    // and intended, so a row-weighted observed flag would sit above 1.0 for a
    // legitimate reason and the gate would be measuring the wrong thing.
    // `generationsFor` is a hash of the CARD KEY and the window bounds alone,
    // so a lift on it can only come from a construction correlation, which is
    // precisely what must be zero. Same reasoning as sub-gate G taking
    // ownership from the world catalogue rather than from the view.
    auto scheduleIt = generationsByCard.find(tx.source);
    if (scheduleIt == generationsByCard.end()) {
      scheduleIt =
          generationsByCard
              .emplace(tx.source, pl::entity::card::reissue::generationsFor(
                                      tx.source, legStartEpoch, legEndEpoch))
              .first;
    }
    const auto [genIt, genInserted] = multiGenerationCard.try_emplace(
        tx.source, scheduleIt->second.size() > 1);
    if (kDisarmFraudDrivenReissue && fraudCards.contains(tx.source)) {
      genIt->second = true;
    }
    if (genIt->second) {
      ++viewMultiGenCard;
      if (fraud) {
        ++viewMultiGenCardFraud;
      }
    }

    // ------- I: the endpoint rollups, on the exporter's own two keys
    const auto generation = pl::entity::card::reissue::generationAt(
        scheduleIt->second, tx.timestamp);
    const auto dayIndex =
        static_cast<std::int32_t>((tx.timestamp - legStartEpoch) / 86'400);
    auto &dev = deviceRollup[tx.session.deviceId];
    dev.accounts.insert(tx.source);
    dev.cards.emplace(tx.source, generation);
    dev.activeDays.insert(dayIndex);
    ++dev.rows;
    dev.ringOwned =
        tx.session.deviceId.ownerType == pl::devices::OwnerType::ring;
    dev.personOwned =
        tx.session.deviceId.ownerType == pl::devices::OwnerType::person;
    auto &addr = ipRollup[tx.session.ipAddress];
    addr.accounts.insert(tx.source);
    addr.cards.emplace(tx.source, generation);
    ++addr.rows;
    /* `ringOwned` on the IP axis is the attacker-pool predicate rather than
     * an owner type: an address carries no `devices::Identity`, and
     * `ownsIp` is the same membership test the rest of this file uses. */
    addr.ringOwned = addr.ringOwned || attackerIp;
    if (const auto holder = router.ownerOf(tx.source); holder.has_value()) {
      auto &person = personRollup[*holder];
      person.accounts.insert(tx.source);
      ++person.rows;
      if (fraud) {
        ++person.fraudRows;
      }
    }
    if (fraud) {
      ++dev.fraudRows;
      ++addr.fraudRows;
    }
  }

  // ------------------------------------------------------ PRECONDITIONS
  // Each of these exists because the corresponding sub-gate would
  // otherwise pass on an empty set. A gate must assert its own
  // precondition.
  check(unauthorizedRows > 0,
        std::string(leg.name) +
            ": the leg must produce unauthorized (card/ATO) fraud rows");
  check(unauthorizedOnAttackerDevice > 0,
        std::string(leg.name) +
            ": unauthorized rows must reach exogenous attacker "
            "infrastructure; zero means the pool is unreachable");
  check(viewRows > 0, std::string(leg.name) + ": the card view must be "
                                              "non-empty");

  const auto deviceFan = summarize(victimsPerDevice);
  const auto ipFan = summarize(victimsPerIp);

  const double sharedDeviceShare =
      deviceFan.endpoints == 0 ? 0.0
                               : static_cast<double>(deviceFan.shared) /
                                     static_cast<double>(deviceFan.endpoints);
  const double sharedIpShare = ipFan.endpoints == 0
                                   ? 0.0
                                   : static_cast<double>(ipFan.shared) /
                                         static_cast<double>(ipFan.endpoints);

  const double casesPerOperator =
      attackers.operatorCount() == 0
          ? 0.0
          : static_cast<double>(unauthorizedOnAttackerDevice) /
                static_cast<double>(attackers.operatorCount());

  std::printf("  A device fan-out: %zu used, shared %.4f, mean %.3f victims, "
              "max %zu\n",
              deviceFan.endpoints, sharedDeviceShare, deviceFan.meanVictims,
              deviceFan.maxVictims);
  std::printf("  A ip     fan-out: %zu used, shared %.4f, mean %.3f victims, "
              "max %zu\n",
              ipFan.endpoints, sharedIpShare, ipFan.meanVictims,
              ipFan.maxVictims);
  std::printf("    (rows per operator %.2f — the world-shape ratio this leg "
              "pins)\n",
              casesPerOperator);

  check(sharedDeviceShare >= kMinSharedDeviceShare,
        std::string(leg.name) +
            ": attacker devices seen by more than one "
            "victim must be >= " +
            std::to_string(kMinSharedDeviceShare) + ", got " +
            std::to_string(sharedDeviceShare) +
            " — cross-victim reuse is THE card-fraud graph signal");
  check(deviceFan.meanVictims >= kMinMeanVictimsPerDevice,
        std::string(leg.name) +
            ": mean victims per attacker device must be "
            ">= " +
            std::to_string(kMinMeanVictimsPerDevice) + ", got " +
            std::to_string(deviceFan.meanVictims));
  check(deviceFan.maxVictims >= kMinMaxVictimsPerDevice,
        std::string(leg.name) +
            ": the fan-out distribution must have a HEAVY TAIL — max victims "
            "per device >= " +
            std::to_string(kMinMaxVictimsPerDevice) + ", got " +
            std::to_string(deviceFan.maxVictims) +
            ". A flat distribution passes a mean gate and carries none of "
            "the structure an alert fires on");
  check(sharedIpShare >= kMinSharedIpShare,
        std::string(leg.name) + ": shared attacker-IP share must be >= " +
            std::to_string(kMinSharedIpShare) + ", got " +
            std::to_string(sharedIpShare));
  check(ipFan.meanVictims >= kMinMeanVictimsPerIp,
        std::string(leg.name) + ": mean victims per attacker IP must be >= " +
            std::to_string(kMinMeanVictimsPerIp) + ", got " +
            std::to_string(ipFan.meanVictims));

  // ------------------------------------------- B operating position
  const double victimEndpointShare =
      static_cast<double>(unauthorizedOnVictimDevice) /
      static_cast<double>(unauthorizedRows);
  std::printf("  B operating position: %zu rows, campaign %zu, burner %zu, "
              "borrowed %zu, terminal %zu, victim-endpoint %zu (share %.4f vs "
              "nominal %.4f; the excess is the unattributable residual)\n",
              unauthorizedRows, unauthorizedOnAttackerDevice,
              unauthorizedOnBurnerDevice, unauthorizedOnBorrowedDevice,
              unauthorizedOnTerminal, unauthorizedOnVictimDevice,
              victimEndpointShare, kVictimEndpointNominal);
  check(victimEndpointShare >= kVictimEndpointShareLo &&
            victimEndpointShare <= kVictimEndpointShareHi,
        std::string(leg.name) + ": victim-endpoint share must land in [" +
            std::to_string(kVictimEndpointShareLo) + ", " +
            std::to_string(kVictimEndpointShareHi) + "], got " +
            std::to_string(victimEndpointShare) +
            ". Above the band means the operator pool is under-sized and "
            "cases are failing to attribute; below means the "
            "remote-access/household branch has stopped firing");

  // ------------------------------------- B' pool sizing, measured
  // The coverage floor is written in mean CONCURRENT campaigns, so that
  // is what gets measured — not the operator COUNT, which the rule
  // deliberately scales with window length to hold concurrency fixed. A
  // precondition that exists to catch a construction failure must
  // measure the construction, never re-derive it.
  double campaignDayTotal = 0.0;
  for (const auto &op : attackers.operators) {
    campaignDayTotal +=
        static_cast<double>(op.campaignLastEpochExcl - op.campaignFirstEpoch) /
        86'400.0;
  }
  const double meanConcurrent =
      campaignDayTotal / static_cast<double>(leg.days);
  // The rule's own INPUT, printed beside its output. The count is derived
  // from an expected campaign length, so when concurrency misses its
  // floor this is the number that says whether the sizing arithmetic or
  // the length distribution is at fault.
  const double meanClippedCampaign =
      attackers.operatorCount() == 0
          ? 0.0
          : campaignDayTotal / static_cast<double>(attackers.operatorCount());
  std::printf("  B. mean concurrent campaigns %.2f (floor 7.00, "
              "window-independent by construction); mean clipped campaign "
              "%.1f days\n",
              meanConcurrent, meanClippedCampaign);
  check(meanConcurrent >= kMeanConcurrentLo &&
            meanConcurrent <= kMeanConcurrentHi,
        std::string(leg.name) + ": mean concurrent campaigns must land in [" +
            std::to_string(kMeanConcurrentLo) + ", " +
            std::to_string(kMeanConcurrentHi) + "], got " +
            std::to_string(meanConcurrent) +
            ". Below it, most case dates have no campaign running and "
            "attribution silently collapses into the victim-endpoint "
            "branch; above it, the case load is spread too thin for the "
            "reuse signal sub-gate A measures");

  // --------------------------------------- C the shortcut, sized
  const double baseRate =
      static_cast<double>(viewFraud) / static_cast<double>(viewRows);
  const double devicePrecision =
      viewDeviceNotOnFile == 0 ? 0.0
                               : static_cast<double>(viewDeviceNotOnFileFraud) /
                                     static_cast<double>(viewDeviceNotOnFile);
  const double ipPrecision = viewIpNotOnFile == 0
                                 ? 0.0
                                 : static_cast<double>(viewIpNotOnFileFraud) /
                                       static_cast<double>(viewIpNotOnFile);
  const double deviceLift = baseRate > 0.0 ? devicePrecision / baseRate : 0.0;
  const double ipLift = baseRate > 0.0 ? ipPrecision / baseRate : 0.0;

  std::printf("  C card view %zu rows, base fraud rate %.6f\n", viewRows,
              baseRate);
  std::printf("    (channel filter alone admits %zu; the membership clause "
              "drops %zu, %.4f — this loop now matches the exporter's filter "
              "exactly)\n",
              channelRows, channelRows - viewRows,
              channelRows == 0 ? 0.0
                               : static_cast<double>(channelRows - viewRows) /
                                     static_cast<double>(channelRows));
  std::printf("    device not-on-file: %zu rows, precision %.6f, lift %.2fx\n",
              viewDeviceNotOnFile, devicePrecision, deviceLift);
  std::printf("    ip     not-on-file: %zu rows, precision %.6f, lift %.2fx\n",
              viewIpNotOnFile, ipPrecision, ipLift);

  check(viewDeviceNotOnFile > 0 && viewIpNotOnFile > 0,
        std::string(leg.name) +
            ": some card-view rows must sit on endpoints the institution "
            "has NOT recorded, or sub-gate C is measuring nothing");
  check(viewDeviceNotOnFile - viewDeviceNotOnFileFraud > 0,
        std::string(leg.name) +
            ": LEGITIMATE rows must appear on un-recorded devices. Without "
            "them 'no Party edge' is again a perfect fraud rule and "
            "Has_Device cannot ship");
  check(devicePrecision <= kMaxNotOnFilePrecision,
        std::string(leg.name) +
            ": 'device not on file => fraud' precision "
            "must stay <= " +
            std::to_string(kMaxNotOnFilePrecision) + ", got " +
            std::to_string(devicePrecision) +
            ". This number was 1.0 for the attacker half of the endpoint "
            "population before this round, which is why the ownership "
            "tables were withheld");
  check(ipPrecision <= kMaxNotOnFilePrecision,
        std::string(leg.name) +
            ": 'ip not on file => fraud' precision must stay <= " +
            std::to_string(kMaxNotOnFilePrecision) + ", got " +
            std::to_string(ipPrecision));
  // The opposite failure mode. A shortcut replaced by pure noise would
  // be a loss of realism, not a win: an un-enrolled endpoint IS riskier
  // in production, and the graph should say so.
  // ------------------------------- C' entity ground truth is non-degenerate
  std::printf("  C. card-view attacker endpoints (overlay positives): "
              "devices %zu, ips %zu\n",
              cardViewAttackerDevices.size(), cardViewAttackerIps.size());
  check(!cardViewAttackerDevices.empty(),
        std::string(leg.name) +
            ": attacker DEVICES must be reachable in the card view, or "
            "cf_Ground_Truth_Label's device positives are back to being the "
            "5 AML ring devices that only enter through a LEGITIMATE "
            "purchase — vanishing and anti-correlated with the label");
  check(!cardViewAttackerIps.empty(),
        std::string(leg.name) +
            ": attacker IPs must be reachable in the card view, for the same "
            "reason");

  // ------------------------- C'' ATTACKER-OPERATED ENDPOINTS ARE NOT PURE
  //
  // A device whose EVERY row is fraud leaks through neighbourhood
  // aggregation even when its degree says nothing: a transductive model
  // that has seen any of its rows has seen the label for all of them. The
  // fan-out shortcut sub-gate I bands is the visible half of that; this is
  // the invisible half.
  //
  // THE SET IS MINTED-OR-COMPROMISED-HOST, AND THAT IS THE WHOLE REASON
  // THIS CHECK CAN FAIL. Measured over the minted set alone the mixed share
  // is 0.0000 on every leg and CANNOT BE ANYTHING ELSE for any level of any
  // mechanism: a borrowed machine is stamped with the CUSTOMER's identity,
  // so `ownsDevice` is false for it by construction. That is the shape of
  // `attacker-infra-2026-07`'s "a count of endpoints is not a measurement of
  // the graph" — a statistic that reads zero because of what it counts, not
  // because of what the world does. `runsFrom` is the world-side predicate
  // that makes the quantity answerable.
  //
  // THE INSTRUMENT WAS WRONG AND THE BAND IS RE-MEASURED AGAINST THE FIXED
  // ONE. The previous numerator admitted every device owned by a compromised
  // host, so an unused link of that host's replacement chain — carrying only
  // its owner's traffic, `fraudRows == 0 < rows` — scored MIXED without an
  // attacker having touched it. The realized share decomposed almost exactly
  // as hostDevicesInView / (mintedInView + hostDevicesInView), i.e. it moved
  // with how many devices a person happens to own. It also over-counted by
  // roughly 2x: the same four legs read 0.3632 / 0.5000 / 0.3920 / 0.3661 on
  // the old instrument against 0.1789 / 0.1925 / 0.2536 / 0.1574 on the
  // fixed one. `merchant-selection-2026-08` rule 13 governs — a band measured
  // against a superseded construction is not a measurement — so the old
  // full-pipeline floor of 0.1452 is retired rather than carried across.
  //
  // THE FLOOR IS MEASURED, NOT DERIVED (rule 6): four seed/shape pairs in
  // this harness read 0.1789 / 0.1925 / 0.2536 / 0.1574, mean 0.1956, sample
  // SD 0.0413, mean - 3.5 SD = 0.0511. Keeping 0.1452 was the alternative and
  // is rejected on purpose: the minimum reading sits 8% above it, which is a
  // margin no cross-seed spread supports.
  //
  // THE MOVE IS DOWNWARD, WHICH IS THE SAFE DIRECTION FOR A HARNESS GAP. The
  // old floor shipped low deliberately, because a band that only holds in the
  // harness that produced it is the trap `party-geography-2026-07` rule 1
  // records. This one is measured in the gate harness alone, so the
  // full-pipeline re-measurement on the corrected instrument is OWED; a floor
  // that moved DOWN cannot red a correct build in the harness it was not
  // measured in, which is what makes shipping it before that safe.
  //
  // BURNERS COUNT AS MINTED HERE, and that makes the check harder rather than
  // easier: a burner is a single-case fingerprint and is pure by
  // construction, so admitting the class lowers the realized share. It is
  // included because it IS attacker infrastructure — the same reason the
  // exporter now labels it — and excluding it would let the round move the
  // statistic by reclassifying rather than by changing the world.
  //
  // THE DISARM IS `compromisedHostShare = 0.0`, which is exactly what the
  // branch did when it drew its host afresh per case. Measured: the operated
  // set collapses to the minted set and the share reads 0.0000 on all four
  // legs, red everywhere. The separation is the whole band, not a margin.
  //
  // PURE DEVICES MUST SURVIVE, so there is deliberately no ceiling. Device
  // and SIM farms carry no consumer traffic and are documented at industrial
  // scale (Europol Operation SIMCARTEL, 2025: 1,200 SIM boxes, 40,000 SIMs);
  // a build with no pure attacker endpoint would be wrong in the other
  // direction.
  {
    std::size_t operated = 0;
    std::size_t operatedMixed = 0;
    for (const auto &[identity, roll] : deviceRollup) {
      const bool minted = attackers.ownsDevice(identity) ||
                          pl::infra::derived::isBurnerDevice(identity);
      /* THE HOST LEG IS `attackerOperated`, NOT `runsFrom`, AND THAT IS A
       * CORRECTION. `runsFrom(owner)` admits EVERY device the compromised
       * host owns — a replacement chain is 1-6 of them and the attacker
       * used one — and each unused one carries only its owner's traffic, so
       * `fraudRows (0) < rows` scored it MIXED. The numerator was therefore
       * counting host-owned devices in the view, and the realized share
       * decomposed almost exactly as hostDevicesInView / (mintedInView +
       * hostDevicesInView): a statistic that moves with how many devices a
       * person happens to own, not with whether attacker rows sit beside
       * legitimate ones.
       *
       * That is `attacker-infra-2026-07`'s own lesson turned on this file:
       * a count of endpoints is not a measurement of the graph. The
       * replacement reads the per-row `operatingPosition` verdict, which
       * already distinguishes an attacker's row on someone else's machine
       * from the owner's own compromise. */
      if (!minted && !roll.attackerOperated) {
        continue;
      }
      ++operated;
      if (roll.fraudRows < roll.rows) {
        ++operatedMixed;
      }
    }
    const double mixedShare = operated ? static_cast<double>(operatedMixed) /
                                             static_cast<double>(operated)
                                       : 0.0;
    std::printf("  C'' attacker-operated devices %zu (compromised-host pool "
                "%zu people), MIXED %zu, share %.4f (floor %.4f)\n",
                operated, attackers.compromisedHosts.size(), operatedMixed,
                mixedShare, kMinOperatedMixedShare);
    check(mixedShare >= kMinOperatedMixedShare,
          std::string(leg.name) +
              ": attacker-operated endpoints must not be purely fraudulent — "
              "mixed share must be at least " +
              std::to_string(kMinOperatedMixedShare) + ", got " +
              std::to_string(mixedShare) +
              ". A borrowed consumer machine has to KEEP its owner's traffic; "
              "moving the attacker's rows off one endpoint and onto another "
              "clean one leaves both pure and closes nothing");
  }

  // ------------------- C''' THE SAME RULE READ PER VERTEX, NOT PER ROW
  //
  // Sub-gate C bands "endpoint not on file => fraud" over ROWS. The exported
  // graph does not present rows for this feature; it presents a `cf_Device`
  // vertex that either has a `cf_Has_Device` edge or does not, and a model
  // asks the question once per vertex. The two weightings are the same
  // predicate on very different denominators — one consumer device carries
  // hundreds of rows while one burner carries a handful — and the per-vertex
  // form is the stronger rule, because every ownerless class is
  // low-row-count. C was passing while the form a GNN actually sees was
  // unbanded. That is `attacker-infra-2026-07`'s lesson once more: the
  // measurement has to be taken on the quantity the consumer can compute.
  //
  // FOUR OWNERLESS CLASSES, AND ONLY TWO OF THEM ARE ATTACKER-SIDE.
  // `Has_Device` is written from `devices.usages` alone, so anything with no
  // Usage row can never receive an edge at any enrollment coverage: public
  // terminals (legitimate, by construction), attacker campaign devices,
  // burner fingerprints, and — on the IP axis — carrier-NAT addresses. The
  // two legitimate classes are what keep this rule from being a synonym for
  // "attacker", and they are why the ceiling can be met without capping
  // anything on the attacker side.
  //
  // THE LIFT FLOOR IS NOT OPTIONAL, for sub-gate C's reason: an un-enrolled
  // endpoint IS riskier in production, and driving this to 1.0 would replace
  // a shortcut with noise (`attacker-infra-2026-07` rule 5).
  //
  // SCOPE: `deviceRollup` is the CARD-VIEW device set, which is the slice a
  // card-fraud model trains on. The full `cf_Device` universe additionally
  // holds world devices that carried no card-view row; they are unlabelled
  // negatives and would only dilute this, so the card-view reading is the
  // pessimistic one.
  {
    std::size_t vertices = 0;
    std::size_t fraudVertices = 0;
    std::size_t ownerless = 0;
    std::size_t ownerlessFraud = 0;
    for (const auto &[identity, roll] : deviceRollup) {
      ++vertices;
      const bool anyFraud = roll.fraudRows > 0;
      if (anyFraud) {
        ++fraudVertices;
      }
      if (!deviceOnFile.contains(identity)) {
        ++ownerless;
        if (anyFraud) {
          ++ownerlessFraud;
        }
      }
    }
    const double vertexBase =
        vertices == 0 ? 0.0
                      : static_cast<double>(fraudVertices) /
                            static_cast<double>(vertices);
    const double ownerlessPrecision =
        ownerless == 0 ? 0.0
                       : static_cast<double>(ownerlessFraud) /
                             static_cast<double>(ownerless);
    const double ownerlessLift =
        vertexBase > 0.0 ? ownerlessPrecision / vertexBase : 0.0;
    std::printf("  C''' device VERTEX no-Has_Device: %zu of %zu ownerless, "
                "precision %.4f, base %.4f, lift %.2fx (ceiling %.4f)\n",
                ownerless, vertices, ownerlessPrecision, vertexBase,
                ownerlessLift, kMaxOwnerlessVertexPrecision);
    check(ownerless > 0,
          std::string(leg.name) +
              ": C''' measures nothing if every card-view device carries an "
              "ownership edge");
    check(ownerlessPrecision <= kMaxOwnerlessVertexPrecision,
          std::string(leg.name) +
              ": 'this Device vertex has no Has_Device edge => fraud' "
              "precision must stay <= " +
              std::to_string(kMaxOwnerlessVertexPrecision) + ", got " +
              std::to_string(ownerlessPrecision) +
              ". Ownership-edge presence is a one-bit feature on every "
              "exported device; if it separates the label it is the "
              "fan-out shortcut wearing a different column");
    check(ownerlessLift > 1.0,
          std::string(leg.name) +
              ": the per-vertex ownership rule must retain LIFT over the "
              "vertex base rate (got " +
              std::to_string(ownerlessLift) +
              "x), for sub-gate C's reason");
  }

  check(deviceLift > 1.0,
        std::string(leg.name) +
            ": endpoint-not-on-file must retain real LIFT over base rate "
            "(got " +
            std::to_string(deviceLift) +
            "x). Killing the shortcut by making the feature pure noise "
            "would be the opposite error");

  // ------------------------------------------------- D point-in-time
  std::printf("  D outside-tenure rows: device %zu, ip %zu (both must be 0)\n",
              attackerRowsOutsideDeviceTenure, attackerRowsOutsideIpTenure);
  check(attackerRowsOutsideDeviceTenure == 0,
        std::string(leg.name) +
            ": every row carrying an attacker device must fall inside that "
            "device's own tenure; got " +
            std::to_string(attackerRowsOutsideDeviceTenure) + " outside");
  check(attackerRowsOutsideIpTenure == 0,
        std::string(leg.name) +
            ": every row carrying an attacker IP must fall inside that "
            "address's own tenure; got " +
            std::to_string(attackerRowsOutsideIpTenure) + " outside");

  // ------------------------------------------- E residential proxy
  const auto attackerDeviceRows = proxyRows + operatorInfraRows;
  const double proxyShare = attackerDeviceRows == 0
                                ? 0.0
                                : static_cast<double>(proxyRows) /
                                      static_cast<double>(attackerDeviceRows);
  std::printf("  E attacker-device rows %zu: own infra %zu, residential proxy "
              "%zu (share %.4f)\n",
              attackerDeviceRows, operatorInfraRows, proxyRows, proxyShare);
  // ------- F social-engineering rails: no attacker endpoint, ever
  std::printf("  F scam-rail (gift-card + impostor push) rows %zu: with an "
              "attacker endpoint %zu (must be 0), sessionless %zu (must be "
              "0)\n",
              scamRailRows, scamRailWithAttackerEndpoint, scamRailSessionless);
  check(scamRailRows > 0,
        std::string(leg.name) +
            ": the leg must produce social-engineering scam rows, or sub-gate "
            "F is vacuous");
  check(scamRailWithAttackerEndpoint == 0,
        std::string(leg.name) +
            ": a telephone scammer produces NO session — "
            "no gift-card or impostor-push row may carry "
            "attacker infrastructure, got " +
            std::to_string(scamRailWithAttackerEndpoint) +
            ". The victim operates their own instrument on these rails");
  check(scamRailSessionless == 0,
        std::string(leg.name) +
            ": every scam row must still carry the VICTIM'S own routed "
            "endpoint — absent attacker infrastructure is not the same as "
            "absent session, and a blank endpoint reaches the card-fraud "
            "sink as a throw");

  const double giftCardOnlineShare =
      giftCardRows == 0 ? 0.0
                        : static_cast<double>(giftCardOnlineRows) /
                              static_cast<double>(giftCardRows);
  std::printf("  F. gift-card channel: %zu rows, online %zu (share %.4f) — "
              "BOTH branches must exist\n",
              giftCardRows, giftCardOnlineRows, giftCardOnlineShare);
  check(giftCardRows > 0,
        std::string(leg.name) + ": the gift-card rail must be exercised");
  check(giftCardOnlineRows > 0,
        std::string(leg.name) +
            ": coached gift-card purchases must include DIGITAL e-gift "
            "codes bought online. This rail was hardcoded card-present, so "
            "the online branch — the one where an issuer has a live session "
            "to interrupt before the codes are read out — did not exist");
  check(giftCardOnlineShare < 0.5,
        std::string(leg.name) +
            ": the in-store physical rack must stay the MAJORITY channel "
            "across the corpus era; got online share " +
            std::to_string(giftCardOnlineShare));

  // ------- G merchant ownership register: structure, not signal
  const double ownedPrecision =
      viewOwnedMerchant == 0 ? 0.0
                             : static_cast<double>(viewOwnedMerchantFraud) /
                                   static_cast<double>(viewOwnedMerchant);
  const double ownedLift = baseRate > 0.0 ? ownedPrecision / baseRate : 0.0;
  std::printf("  G merchant register: %zu of %zu catalogue merchants owned; "
              "%zu card-view rows on an owned merchant, fraud rate %.6f, "
              "lift %.3fx (must sit on 1.0)\n",
              ownerByMerchant.size(),
              result.counterparties.merchants.records.size(), viewOwnedMerchant,
              ownedPrecision, ownedLift);
  /* G' THE MERCHANT-LEVEL FORM, and it is now the one that is BANDED.
   *
   * The row-weighted lift above asks "what share of rows landing on an owned
   * merchant are fraud, against the base rate". That question confounds TWO
   * independent things: whether the ownership hash correlates with being a
   * fraud destination — the construction claim, which is what this gate is
   * for — and how heavily fraud rows CONCENTRATE on a few merchants, which is
   * a property of the fraud rail and has nothing to do with ownership.
   *
   * The confound is not small. With ~343 merchants and ~4,000 fraud rows
   * spread over a heavy-tailed destination pool, whether ONE busy fraud
   * merchant happens to fall on the owned side of a 45% coin moves the row
   * ratio by tens of percent. The effective sample is a handful of merchants,
   * not 326,000 rows, so a row-level binomial understates the variance by
   * more than an order of magnitude — CLAUDE.md card-churn rule 3, the same
   * clustering trap, in a form where it is far more severe.
   *
   * This form asks the construction question directly: of the DISTINCT
   * merchants fraud reached, what share carry an owner edge, against the
   * share among all view merchants? One entry per merchant, so the unit
   * matches the coin the hash actually flips. */
  const double ownedShareAll =
      viewMerchantSet.empty()
          ? 0.0
          : static_cast<double>(std::count_if(
                viewMerchantSet.begin(), viewMerchantSet.end(),
                [&](const pl::entity::Key &k) {
                  return ownerByMerchant.find(k) != ownerByMerchant.end();
                })) /
                static_cast<double>(viewMerchantSet.size());
  const double ownedShareFraud =
      viewFraudMerchantSet.empty()
          ? 0.0
          : static_cast<double>(std::count_if(
                viewFraudMerchantSet.begin(), viewFraudMerchantSet.end(),
                [&](const pl::entity::Key &k) {
                  return ownerByMerchant.find(k) != ownerByMerchant.end();
                })) /
                static_cast<double>(viewFraudMerchantSet.size());
  const double merchantLift =
      ownedShareAll > 0.0 ? ownedShareFraud / ownedShareAll : 0.0;

  /* G'' THE CONSTRUCTION RULE ITSELF, tested directly rather than through a
   * consequence — and it is here because the CONSEQUENCE form cannot see the
   * leak that matters.
   *
   * `ownership::onFile` hashes the merchant key alone, so coverage must be the
   * SAME on either side of any other merchant attribute. Footprint is the
   * attribute that matters: the card rail is heavily card-not-present and
   * draws only from `Footprint::online`, so a register that preferred
   * physical outlets would make "has an owner edge" a modality flag and, with
   * it, a label flag.
   *
   * MEASURED WHY THIS EXISTS: driving that exact leak (`PL_OWNERSHIP_DISARM`,
   * ownership = physical-only) leaves the merchant-level fraud lift at
   * 0.9337-0.9683 — INSIDE any band its own armed readings would justify. The
   * consequence form is ceiling-compressed once coverage reaches 86%, so it
   * cannot separate. This form reads 0.0 against ~1.0 and separates
   * completely. CLAUDE.md merchant-selection rule 6, third instance: a band
   * whose disarm passes is not a check. */
  std::size_t onlineSeen = 0;
  std::size_t onlineOwned = 0;
  std::size_t physicalSeen = 0;
  std::size_t physicalOwned = 0;
  for (const auto &key : viewMerchantSet) {
    const auto fit = footprintByKey.find(key);
    if (fit == footprintByKey.end()) {
      continue;
    }
    const bool owned = ownerByMerchant.find(key) != ownerByMerchant.end();
    if (fit->second == pl::entity::merchant::Footprint::online) {
      ++onlineSeen;
      onlineOwned += owned ? 1 : 0;
    } else {
      ++physicalSeen;
      physicalOwned += owned ? 1 : 0;
    }
  }
  const double onlineCoverage =
      onlineSeen == 0 ? 0.0
                      : static_cast<double>(onlineOwned) /
                            static_cast<double>(onlineSeen);
  const double physicalCoverage =
      physicalSeen == 0 ? 0.0
                        : static_cast<double>(physicalOwned) /
                              static_cast<double>(physicalSeen);
  const double modalityRatio =
      physicalCoverage > 0.0 ? onlineCoverage / physicalCoverage : 0.0;
  std::printf("  G'' modality independence: online %zu (owned %.4f), physical "
              "%zu (owned %.4f), ratio %.4fx\n",
              onlineSeen, onlineCoverage, physicalSeen, physicalCoverage,
              modalityRatio);
  std::printf("  G' merchant-level: %zu view merchants (owned share %.4f), "
              "%zu fraud-touched (owned share %.4f), lift %.4fx\n",
              viewMerchantSet.size(), ownedShareAll,
              viewFraudMerchantSet.size(), ownedShareFraud, merchantLift);

  // IS THE REGISTER DEGENERATE? Coverage alone does not answer that. A
  // register where one Party owns every merchant would satisfy a
  // non-emptiness check, satisfy the loader, and be a single hub vertex
  // that tells a model nothing — the same shape as the endpoint defect
  // this file exists for. So measure the OWNER-SIDE distribution too:
  // most proprietors hold one outlet, a few hold several.
  std::map<pl::entity::PersonId, std::size_t> merchantsPerOwner;
  for (const auto &[merchantKey, owner] : ownerByMerchant) {
    ++merchantsPerOwner[owner];
  }
  std::size_t maxPerOwner = 0;
  for (const auto &[owner, count] : merchantsPerOwner) {
    maxPerOwner = std::max(maxPerOwner, count);
  }
  const double meanPerOwner =
      merchantsPerOwner.empty()
          ? 0.0
          : static_cast<double>(ownerByMerchant.size()) /
                static_cast<double>(merchantsPerOwner.size());
  std::printf("  G. register shape: %zu distinct proprietors, mean %.2f "
              "merchants each, max %zu\n",
              merchantsPerOwner.size(), meanPerOwner, maxPerOwner);
  check(!ownerByMerchant.empty(),
        std::string(leg.name) +
            ": the merchant ownership register must be non-empty, or "
            "cf_Is_Merchant is empty and the downstream loader aborts");
  check(merchantsPerOwner.size() >= ownerByMerchant.size() / 3,
        std::string(leg.name) +
            ": the register must spread over MANY proprietors — got " +
            std::to_string(merchantsPerOwner.size()) + " owners for " +
            std::to_string(ownerByMerchant.size()) +
            " merchants. A register concentrated on a few Parties is one hub "
            "vertex wearing an ownership table");
  check(maxPerOwner <= 6,
        std::string(leg.name) +
            ": no proprietor should hold an implausible number of outlets; "
            "max " +
            std::to_string(maxPerOwner));
  check(viewOwnedMerchant > 0,
        std::string(leg.name) +
            ": card-view rows must reach owned merchants, or sub-gate G "
            "measures nothing");
  /* THE ROW-WEIGHTED LIFT IS PRINTED AND NO LONGER BANDED, and retiring that
   * band is a correction rather than a relaxation.
   *
   * It used to assert `0.80 < ownedLift < 1.25`, sized on TWO readings (1.122
   * and 0.950) of a superseded leg set. Re-measured over the four legs this
   * file now runs: 1.345 / 1.249 / 1.495 / 0.581. Three of four fail — while
   * the construction is provably clean, since `ownership::onFile` hashes role,
   * bank and number and reads nothing else.
   *
   * WHY THE STATISTIC IS WRONG, not the world. It divides a fraud rate over
   * ROWS landing on owned merchants by the corpus base rate, which confounds
   * the construction question — does the ownership hash correlate with being a
   * fraud destination — with how heavily fraud rows CONCENTRATE on a few
   * merchants, a property of the fraud rail that has nothing to do with
   * ownership. With ~343 merchants and fraud spread over a heavy-tailed
   * destination pool, whether ONE busy fraud merchant lands on the owned side
   * of a 45% coin moves the ratio by tens of percent. The effective sample is
   * a handful of merchants, not the 326,000 rows the denominator suggests, so
   * a row-level binomial understates the variance by more than an order of
   * magnitude. Same clustering trap as CLAUDE.md card-churn rule 3, in a far
   * more severe form, and the same lesson as venue-reuse rule 3: the pairwise
   * statistic was the wrong one.
   *
   * As a DETECTOR it was broken in both directions: it fired on 3 of 4 clean
   * legs, and its one true positive (the footprint disarm, 0.72-0.78) is
   * caught with total separation by G'' below. G' and G'' replace it. */
  std::printf("  G  (row-weighted lift %.3fx — PRINTED, confounded by "
              "fraud-row concentration; G'/G'' carry the check)\n",
              ownedLift);

  /* G' THE WEIGHT LEAK. Merchant-level owned-share DIFFERENCE, not the ratio:
   * the ratio is ceiling-compressed once coverage is high and cannot separate.
   *
   * MEASURED armed: +0.0059 / +0.0161 / +0.0487 / -0.0120, so the sign flips
   * and the magnitude stays under 0.05.
   * DISARM `PL_OWNERSHIP_DISARM=weight` (register keyed on popularity, the
   * other attribute CLAUDE.md merchant-ownership rule 1 names): +0.1162 /
   * +0.1146 / +0.1072 / +0.1251 — all four outside, minimum 0.1072.
   *
   * The bound sits between armed max 0.0487 and disarm min 0.1072: 1.64x
   * headroom above the clean readings, 1.34x below the leak. Thin on the
   * disarm side and stated rather than hidden — the honest reading is that
   * this catches a FULL weight keying and would miss a weak one. G'' is the
   * high-margin half of the pair. */
  const double ownedShareGap = ownedShareFraud - ownedShareAll;
  check(std::abs(ownedShareGap) <= kMaxOwnedShareGap,
        std::string(leg.name) +
            ": fraud-touched merchants carry owner edges at a different rate "
            "than view merchants overall — gap " +
            std::to_string(ownedShareGap) + " exceeds " +
            std::to_string(kMaxOwnedShareGap) +
            ". Register membership is a hash of the merchant key alone "
            "precisely so this holds; a rule reading weight would put the "
            "register on the destination side of every fraud row");

  /* G'' THE FOOTPRINT LEAK, and the high-margin half.
   *
   * MEASURED armed: 1.0632 / 0.9268 / 1.2756 / 0.8104, mean 1.0190, sample SD
   * 0.2000, mean +/- 3.5 SD = [0.319, 1.719]. Rounded outward. The band is
   * wide because only 40-59 view merchants are online — a structural limit,
   * not a choice, and the observed SD (0.200) tracks its own binomial
   * prediction (0.175), so this is noise and not residual signal.
   *
   * DISARM `PL_OWNERSHIP_DISARM` (register = physical outlets only, the
   * tempting version of the feature): 0.0000 on all four legs. Total
   * separation — 2.5x below the lower band edge and 0 against an armed
   * minimum of 0.81. */
  check(modalityRatio >= kModalityRatioLo && modalityRatio <= kModalityRatioHi,
        std::string(leg.name) +
            ": owner-edge coverage differs by FOOTPRINT — online/physical "
            "ratio " +
            std::to_string(modalityRatio) + " outside [" +
            std::to_string(kModalityRatioLo) + ", " +
            std::to_string(kModalityRatioHi) +
            "]. The card rail draws card-not-present rows only from "
            "Footprint::online, so a register that prefers either modality "
            "becomes a modality flag and with it a label flag");

  // ------- H reissue schedule: cardinality, not signal
  const double multiGenPrecision =
      viewMultiGenCard == 0 ? 0.0
                            : static_cast<double>(viewMultiGenCardFraud) /
                                  static_cast<double>(viewMultiGenCard);
  const double multiGenLift =
      baseRate > 0.0 ? multiGenPrecision / baseRate : 0.0;
  std::size_t multiGenCards = 0;
  for (const auto &[key, multi] : multiGenerationCard) {
    if (multi) {
      ++multiGenCards;
    }
  }
  std::printf("  H reissue schedule: %zu of %zu view cards reissue in-window; "
              "%zu rows on a reissued card, fraud rate %.6f, lift %.3fx (must "
              "sit on 1.0)\n",
              multiGenCards, multiGenerationCard.size(), viewMultiGenCard,
              multiGenPrecision, multiGenLift);
  check(multiGenCards > 0,
        std::string(leg.name) +
            ": some card must be reissued inside the leg window, or sub-gate "
            "H measures nothing. The pre-round state was exactly ONE card "
            "number per account for the whole run, which scores 0 here");
  check(multiGenCards < multiGenerationCard.size(),
        std::string(leg.name) +
            ": not every card may reissue in-window either — with no "
            "single-generation cards the lift below has no complement to "
            "compare against and passes vacuously");
  check(viewMultiGenCard > 0,
        std::string(leg.name) +
            ": reissued cards must carry card-view rows, or sub-gate H "
            "measures nothing");
  // THE BAND IS MEASURED, NOT INHERITED FROM SUB-GATE G. Eight readings over
  // four seed pairs — 1.012, 0.990, 0.965, 0.953, 1.007, 1.063, 0.987,
  // 0.954 — give mean 0.991 and SD 0.0366. That SD is 2.4x the naive
  // binomial estimate (0.015) because fraud rows CLUSTER: a compromise
  // produces several charges on one card, so the effective sample is cases
  // and not rows. Sizing this band off the binomial would have produced a
  // gate that flakes, which is the same "derived instead of measured" mistake
  // the concurrency floor made twice.
  //
  // The ceiling sits at 3.5 SD above the observed mean, which is above the
  // weaker of the two disarm readings (1.153) and well below the stronger
  // (1.478). Fraud-driven reissue is the leak this forbids: in production a
  // replacement card is very often a CONSEQUENCE of compromise, so wiring
  // `seen.fraud` into the schedule would put a full-window verdict into an
  // exported vertex count. `generationsFor` reads the card key and the window
  // and nothing else, which is what holds this at 1.0.
  check(multiGenLift > 0.87 && multiGenLift < 1.13,
        std::string(leg.name) +
            ": 'the cardholder was reissued a card' must carry NO SYSTEMATIC "
            "fraud signal — lift must straddle 1.0, got " +
            std::to_string(multiGenLift) +
            ". Card cardinality is now an exported feature (one cf_Card "
            "vertex per observed generation), so a reissue rule that read the "
            "fraud flag would turn a row count into the label");

  check(proxyShare >= kMinProxyShare && proxyShare <= kMaxProxyShare,
        std::string(leg.name) + ": residential-proxy share must land in [" +
            std::to_string(kMinProxyShare) + ", " +
            std::to_string(kMaxProxyShare) + "], got " +
            std::to_string(proxyShare) +
            ". This is the mechanism that puts a fraud transaction one hop "
            "from an unrelated Party through a shared address");

  // ------- I degree and label, on both keys and both endpoint axes
  //
  // DISARM SWITCH for I.3, and it is here for the reason sub-gate H's is:
  // I.3 already passes on the pre-fix tree, and a check that has never been
  // made to fail is indistinguishable from one that cannot. Flipping this to
  // true lifts every fraud row off the fan-out-1 bucket and onto the single
  // widest-reaching device, which is what "all fraud is carried by shared
  // infrastructure" looks like — the pre-round world with the victim-endpoint
  // branch removed.
  //
  // MEASURED with it on, all four legs: fraud at fan-out 1 falls to 0 (I.3
  // red x4) and I.2 reds on both keys, account 0.5545 / 0.4904 / 0.4063 /
  // 0.4459 against 0.0368 / 0.0582 / 0.0271 / 0.0483 armed — 12 checks red.
  //
  // AN EARLIER PASS OF THIS ROUND ALSO CLAIMED I.6 REDS HERE. IT DOES NOT,
  // and the reason is worth keeping: the device this switch piles fraud onto
  // is the widest one in the corpus, which is a public terminal, and a
  // terminal is MIXED — so the mixed-endpoint share is unmoved or improved
  // by concentrating fraud there. I.6's own disarm is
  // `kDisarmInstrumentCeiling` below, which deletes that population outright.
  constexpr bool kDisarmFraudOffLowDegree = false;
  if (kDisarmFraudOffLowDegree) {
    EndpointRollup *widest = nullptr;
    for (auto &[id, roll] : deviceRollup) {
      if (widest == nullptr || roll.accounts.size() > widest->accounts.size()) {
        widest = &roll;
      }
    }
    if (widest != nullptr) {
      for (auto &[id, roll] : deviceRollup) {
        if (&roll == widest || roll.accounts.size() > 1 ||
            roll.fraudRows == 0) {
          continue;
        }
        widest->rows += roll.fraudRows;
        widest->fraudRows += roll.fraudRows;
        roll.rows -= roll.fraudRows;
        roll.fraudRows = 0;
      }
    }
  }

  /* DISARM SWITCH for I.1, I.2, I.4, I.5 and I.5b — THE CEILING HALF.
   *
   * The pre-round tree cannot be linked against this file (it predates
   * `public_endpoints.hpp` and `derived_endpoints.hpp`, and the rollup reads
   * both), so the disarm reconstructs the pre-round world's ONE defining
   * property inside the measurement instead: a person could present at most
   * two card-view instruments, every legitimate device belonged to exactly
   * one person, and no shared or public line carried traffic. Truncating a
   * non-attacker endpoint's partner set to two and deleting the endpoints
   * that belong to nobody in particular is exactly that world, read off the
   * current corpus.
   *
   * MEASURED WITH IT ON, ALL FOUR LEGS — 36 checks red, and what it
   * reproduces is the reported symptom rather than merely a failure:
   *
   *   I.1  firstAllFraudK  31 33 31 33  ->  3 3 3 3
   *   I.2  best precision, account key AND card key, identical on both
   *                        0.0368 0.0582 0.0271 0.0483
   *                     -> 0.9432 1.0000 0.9367 0.9580, at k >= 3 / 19 / 3 / 3
   *   I.2  best precision, IP axis
   *                        0.0085 0.0088 0.0085 0.0081  ->  1.0000 x4
   *   I.4  Fano            21.407 19.078 26.553 22.310
   *                     -> 0.2588 0.3042 0.4769 0.3145
   *   I.5  non-ring p99    38 49 39 50  ->  2 2 2 2
   *   I.5b consumer p99     7  7  7  7  ->  2 2 2 2
   *   I.6  mixed share     0.6126 0.5659 0.5975 0.5590
   *                     -> 0.3563 0.3414 0.3864 0.3486
   *   I.8  burst precision 0.0476 0.0509 0.0647 0.0455
   *                     -> 0.6533 0.6658 0.6262 0.6762
   *
   * The disarmed I.4 lands inside the nine pre-fix readings recorded at that
   * check (0.166-0.938) and the disarmed I.8 lands on the two recorded there
   * (0.7214 and 0.6229). That is the evidence this reconstruction IS the
   * pre-round world and not merely something that fails.
   *
   * I.3 does not red here and should not — fraud at fan-out 1 survives a
   * ceiling untouched, and `kDisarmFraudOffLowDegree` above is its switch.
   *
   * TWO OF THE REDS ARE SHARED CREDIT AND SAYING SO IS THE POINT. I.6 and
   * I.8 move because the switch also DELETES the endpoints belonging to
   * nobody in particular, and those carry both mixed traffic and steady
   * day-over-day rows. So this is the whole CEILING HALF of the defect —
   * per-person instruments and the shared/public population together — not a
   * degree truncation, and I.6's and I.8's reds here must not be read as
   * attributable to cards-per-person alone. I.5b is the check that isolates
   * that term. */
  constexpr bool kDisarmInstrumentCeiling = false;
  if (kDisarmInstrumentCeiling) {
    for (auto it = deviceRollup.begin(); it != deviceRollup.end();) {
      if (!it->second.ringOwned && !it->second.personOwned) {
        it = deviceRollup.erase(it);
        continue;
      }
      if (!it->second.ringOwned) {
        while (it->second.accounts.size() > 2) {
          it->second.accounts.erase(std::prev(it->second.accounts.end()));
        }
        while (it->second.cards.size() > 2) {
          it->second.cards.erase(std::prev(it->second.cards.end()));
        }
      }
      ++it;
    }
    for (auto &[addr, roll] : ipRollup) {
      while (roll.accounts.size() > 2 && !roll.ringOwned) {
        roll.accounts.erase(std::prev(roll.accounts.end()));
      }
    }
  }

  std::vector<DegreePoint> deviceByAccount;
  std::vector<DegreePoint> deviceByCard;
  std::vector<std::size_t> legitDeviceDegrees;
  std::vector<std::size_t> consumerDeviceDegrees;
  std::size_t mixedDeviceFraud = 0;
  std::size_t burstRows = 0;
  std::size_t burstFraud = 0;
  deviceByAccount.reserve(deviceRollup.size());
  deviceByCard.reserve(deviceRollup.size());
  for (const auto &[id, roll] : deviceRollup) {
    deviceByAccount.push_back(
        {roll.accounts.size(), roll.rows, roll.fraudRows});
    deviceByCard.push_back({roll.cards.size(), roll.rows, roll.fraudRows});
    /* NON-ATTACKER means NOT `OwnerType::ring`, which excludes campaign
     * devices, burners AND the handful of AML-ring SharedInfra devices in one
     * predicate. The ring devices are excluded deliberately: they reach the
     * card view only through a ring member's LEGITIMATE purchase, their count
     * is uncontrolled, and it would not survive a change to ring sizing — so
     * they are the wrong thing to band a legitimate-side floor against even
     * though they are the highest-degree legitimate devices in the corpus. */
    if (!roll.ringOwned) {
      legitDeviceDegrees.push_back(roll.accounts.size());
    }
    if (roll.personOwned) {
      consumerDeviceDegrees.push_back(roll.accounts.size());
    }
    if (roll.fraudRows > 0 && roll.rows > roll.fraudRows) {
      mixedDeviceFraud += roll.fraudRows;
    }
    const auto activeDays = std::max<std::size_t>(roll.activeDays.size(), 1);
    if (static_cast<double>(roll.rows) / static_cast<double>(activeDays) >=
        3.0) {
      burstRows += roll.rows;
      burstFraud += roll.fraudRows;
    }
  }

  std::vector<DegreePoint> ipByAccount;
  std::size_t mixedIpFraud = 0;
  ipByAccount.reserve(ipRollup.size());
  for (const auto &[addr, roll] : ipRollup) {
    ipByAccount.push_back({roll.accounts.size(), roll.rows, roll.fraudRows});
    if (roll.fraudRows > 0 && roll.rows > roll.fraudRows) {
      mixedIpFraud += roll.fraudRows;
    }
  }

  const auto accountSweep = degreeLabelSweep(deviceByAccount, viewFraud);
  const auto cardSweep = degreeLabelSweep(deviceByCard, viewFraud);
  const auto ipSweep = degreeLabelSweep(ipByAccount, viewFraud);
  const auto legitP99 = percentileDegree(legitDeviceDegrees, 0.99);
  const auto consumerP99 = percentileDegree(consumerDeviceDegrees, 0.99);
  const auto consumerMax =
      consumerDeviceDegrees.empty()
          ? std::size_t{0}
          : *std::ranges::max_element(consumerDeviceDegrees);
  const double mixedDeviceShare = viewFraud == 0
                                      ? 0.0
                                      : static_cast<double>(mixedDeviceFraud) /
                                            static_cast<double>(viewFraud);
  const double mixedIpShare =
      viewFraud == 0
          ? 0.0
          : static_cast<double>(mixedIpFraud) / static_cast<double>(viewFraud);
  const double burstPrecision =
      burstRows == 0
          ? 0.0
          : static_cast<double>(burstFraud) / static_cast<double>(burstRows);
  const double burstLift = baseRate > 0.0 ? burstPrecision / baseRate : 0.0;
  const double accountLift =
      baseRate > 0.0 ? accountSweep.bestPrecision / baseRate : 0.0;
  const double cardLift =
      baseRate > 0.0 ? cardSweep.bestPrecision / baseRate : 0.0;
  const double ipLiftAtBest =
      baseRate > 0.0 ? ipSweep.bestPrecision / baseRate : 0.0;

  std::printf("  I device fan-out (account key): %zu endpoints, mean %.3f, max "
              "%zu, Fano %.3f, firstAllFraudK %zu, best rule k>=%zu precision "
              "%.4f recall %.4f lift %.2fx, fraud rows at k=1 %zu\n",
              accountSweep.endpoints, accountSweep.meanDegree,
              accountSweep.maxDegree, accountSweep.fano,
              accountSweep.firstAllFraudK, accountSweep.bestK,
              accountSweep.bestPrecision, accountSweep.bestRecall, accountLift,
              accountSweep.fraudAtDegreeOne);
  std::printf("  I device fan-out (card key):    %zu endpoints, mean %.3f, max "
              "%zu, Fano %.3f, firstAllFraudK %zu, best rule k>=%zu precision "
              "%.4f recall %.4f lift %.2fx\n",
              cardSweep.endpoints, cardSweep.meanDegree, cardSweep.maxDegree,
              cardSweep.fano, cardSweep.firstAllFraudK, cardSweep.bestK,
              cardSweep.bestPrecision, cardSweep.bestRecall, cardLift);
  std::printf("  I ip     fan-out (account key): %zu endpoints, mean %.3f, max "
              "%zu, Fano %.3f, firstAllFraudK %zu, best rule k>=%zu precision "
              "%.4f recall %.4f lift %.2fx\n",
              ipSweep.endpoints, ipSweep.meanDegree, ipSweep.maxDegree,
              ipSweep.fano, ipSweep.firstAllFraudK, ipSweep.bestK,
              ipSweep.bestPrecision, ipSweep.bestRecall, ipLiftAtBest);
  std::printf("  I legitimate side: %zu non-ring devices, p99 fan-out %zu; "
              "%zu CONSUMER devices, p99 %zu, max %zu; "
              "mixed-endpoint fraud share device %.4f, ip %.4f\n",
              legitDeviceDegrees.size(), legitP99, consumerDeviceDegrees.size(),
              consumerP99, consumerMax, mixedDeviceShare, mixedIpShare);
  std::printf("  I temporal: 'rows per active day >= 3' %zu rows, precision "
              "%.4f, lift %.2fx\n",
              burstRows, burstPrecision, burstLift);

  check(accountSweep.endpoints > 0 && ipSweep.endpoints > 0 && viewFraud > 0,
        std::string(leg.name) +
            ": the card view must carry endpoints AND fraud, or every check "
            "in sub-gate I passes on an empty set");

  // I.1 --- NO ALL-FRAUD FAN-OUT BUCKET DOWN WHERE THE MASS IS.
  //
  // The pre-fix tree reads 7 at ALL FIVE configurations measured (two gate
  // legs, pop 1,800x730, pop 6,000x730, pop 20,000x365) while devices exist
  // out to k = 26/27/44 — so the tail was pure fraud from a third of the way
  // up, and that is the shape a threshold rule converts into precision 1.0.
  //
  // Shipping readings over the four pairs: 31 / 33 / 31 / 33, mean 32.00, SD
  // 1.155, so mean - 3.5 SD is 27.96. THE FLOOR IS HELD BELOW THAT ON
  // PURPOSE. This is a location in a tail whose LENGTH tracks the
  // public-terminal count, so a leg-measured band on it would red on a world
  // that merely reshuffled its tail; the claim being made is the structural
  // one, "no all-fraud bucket down where the mass is". The margin is on the
  // right side of both references that matter — pre-fix 7, disarm 3.
  check(accountSweep.firstAllFraudK == 0 ||
            accountSweep.firstAllFraudK >= kMinFirstAllFraudK,
        std::string(leg.name) +
            ": the first fan-out bucket in which EVERY device is pure fraud "
            "must sit above " +
            std::to_string(kMinFirstAllFraudK) + ", got " +
            std::to_string(accountSweep.firstAllFraudK) +
            ". A low all-fraud bucket is a threshold rule with precision 1.0 "
            "waiting to be learned, and it is what this sub-gate exists for");

  // I.2 --- THE ANTI-SHORTCUT CEILING, MIRRORING SUB-GATE C.
  //
  // Both keys, because they are not the same rule: the ACCOUNT key is the
  // stronger form (pre-fix 0.9093 at pop 20,000 and 0.9749 on leg-long) and a
  // fix that only smeared the exported CARD key across reissue generations
  // would pass a card-only check while leaving the world-side cliff intact.
  //
  // AND THE LIFT FLOOR IS THE OTHER HALF OF THE BAND, exactly as sub-gate C
  // argues for not-on-file: fan-out IS a real risk signal, and a build that
  // scored the ceiling by making degree pure noise would have replaced a
  // shortcut with nothing, which is the opposite error.
  //
  // Shipping readings, account key / card key, over the four pairs:
  // 0.0368 0.0418 / 0.0582 0.0582 / 0.0271 0.0272 / 0.0483 0.0483. Account
  // key mean 0.0426, SD 0.0135, so the tight ceiling would be 0.0900 — see
  // the constant block for why it is not taken.
  //
  // THE IP AXIS PASSES THIS FOR A DIFFERENT REASON AND THE DIFFERENCE IS
  // REGISTERED, NOT PAPERED OVER. The best real IP threshold now scores
  // 0.0085 / 0.0088 / 0.0085 / 0.0081 at LIFT 0.88 / 0.94 / 0.88 / 0.89 —
  // consistently BELOW 1.0, i.e. a high-fan-out address is now slightly
  // SAFER than average, where before this round the same rule scored
  // 0.969-1.000 at k = 5. The mechanism is carrier NAT: it carries ~30% of
  // rows on label-neutral pools with degree in the hundreds, so it fills the
  // high-k buckets with legitimate traffic. There is deliberately NO IP lift
  // floor: 0.88-0.94 does not straddle 1.0, so a straddle check would red,
  // and banding a quantity at its measured inversion would pin the
  // inversion rather than gate it. It is PRINTED and left to the owner.
  check(accountSweep.bestPrecision <= kMaxDegreePrecision,
        std::string(leg.name) +
            ": best 'device fan-out >= k => fraud' precision on the ACCOUNT "
            "key must stay <= " +
            std::to_string(kMaxDegreePrecision) + ", got " +
            std::to_string(accountSweep.bestPrecision) +
            " at k >= " + std::to_string(accountSweep.bestK) +
            ". This was 1.0000 at 33-36% recall before this round");
  check(cardSweep.bestPrecision <= kMaxDegreePrecision,
        std::string(leg.name) +
            ": best 'device fan-out >= k => fraud' precision on the exported "
            "CARD key must stay <= " +
            std::to_string(kMaxDegreePrecision) + ", got " +
            std::to_string(cardSweep.bestPrecision));
  check(ipSweep.bestPrecision <= kMaxDegreePrecision,
        std::string(leg.name) +
            ": best 'ip fan-out >= k => fraud' precision must stay <= " +
            std::to_string(kMaxDegreePrecision) + ", got " +
            std::to_string(ipSweep.bestPrecision) +
            ". Closing the device axis alone relocates the shortcut here — "
            "the IP form already scored 0.969-1.000 at k=5");
  check(accountLift > 1.0,
        std::string(leg.name) +
            ": device fan-out must retain real LIFT over the base rate (got " +
            std::to_string(accountLift) +
            "x). A high-degree endpoint IS riskier in production; scoring the "
            "ceiling by making degree pure noise would be the opposite error");

  // I.3 --- FRAUD MUST APPEAR AT FAN-OUT 1. TRIPWIRE, and labelled one.
  //
  // This ALREADY PASSED on the pre-fix tree — 357 fraud rows in the k=1
  // bucket at pop 20,000, 783 on leg-long — because the victim-endpoint
  // branch has always left `plan.device` unassigned and let the victim's own
  // routed session stand. It is gated anyway because the branch is one
  // `plan.device.assigned()` check away from disappearing, and its absence
  // would make "the device is shared" a necessary condition for fraud. See
  // `kDisarmFraudOffLowDegree` above for the switch that reds it.
  check(accountSweep.fraudAtDegreeOne > 0,
        std::string(leg.name) +
            ": fraud must appear on SINGLE-CARD devices, got " +
            std::to_string(accountSweep.fraudAtDegreeOne) +
            " rows. The highest-value card-not-present attackers buy a fresh "
            "fingerprint per attempt and burn it, so the real curve is "
            "bimodal; a model that learns 'fan-out 1 is safe' has learned an "
            "artifact of the generator");

  // I.4 --- DISPERSION, VIA THE FANO FACTOR.
  //
  // Deliberately NOT an empty-bucket count: a real power-law tail has gaps at
  // the top by small-sample chance and Fano does not punish that. The
  // precedent is `merchant-selection-2026-08` rule 5, which used exactly this
  // statistic to separate a flat national law (Fano ~2) from real locality
  // (8.9-35.2).
  //
  // Fano < 1 is the signature of a near-binomial with a hard ceiling, which
  // is precisely what a two-instrument cap produces. Pre-fix: 0.166 (pop
  // 2,000), 0.183/0.212 (pop 20,000), 0.211, 0.233, 0.364, 0.407, and
  // 0.813/0.938 on these two legs — nine runs, every one of them under 1.0.
  //
  // Shipping readings over the four pairs: 21.407 / 19.078 / 26.553 /
  // 22.310, mean 22.337, SD 3.123, so the floor is 11.40. The brief's
  // declared target was 1.50 and that was what this constant held until it
  // was measured — a floor 15x below the realized value, which the disarm
  // (0.253-0.259) and the pre-fix readings both clear by an order of
  // magnitude, so it could not have seen a PARTIAL regression. The card key
  // reads 25.128 / 21.472 / 30.319 / 23.445 and is printed, not banded.
  check(accountSweep.fano >= kMinDegreeFano,
        std::string(leg.name) + ": cards-per-device Fano factor must be >= " +
            std::to_string(kMinDegreeFano) + ", got " +
            std::to_string(accountSweep.fano) +
            ". Under-dispersion is the signature of a hard per-person "
            "instrument ceiling, and it is what puts a cliff between the "
            "legitimate population and attacker infrastructure");

  // I.5 --- THE LEGITIMATE SIDE MUST REACH.
  //
  // The ceiling half of the defect. Pre-fix the maximum fan-out of ANY
  // non-ring device was 2 (person-owned) or 3 (a legitShared device, exactly
  // once across two legs), so this failed by construction. It is a p99 rather
  // than a maximum because a maximum is one device and would track the tail's
  // luckiest draw.
  //
  // Shipping readings: 38 / 49 / 39 / 50, mean 44.0, SD 6.272, floor 22.
  // This constant held 4 until it was measured, and 4 was reachable by the
  // shared and public population ALONE.
  check(legitP99 >= kMinLegitP99Degree,
        std::string(leg.name) +
            ": p99 cards-per-device over NON-ring devices must be >= " +
            std::to_string(kMinLegitP99Degree) + ", got " +
            std::to_string(legitP99) +
            ". Raising the attacker side is not an option here — capping "
            "attacker fan-out reds sub-gate A's heavy-tail floor — so the "
            "legitimate population is what has to reach the attacker range");

  // I.5b --- AND IT MUST REACH ON THE CONSUMER'S OWN MACHINE.
  //
  // I.5 ALONE CANNOT SEE THE DEFECT THIS ROUND CLOSED, and that is why this
  // check exists. Its population is "not `OwnerType::ring`", which also
  // holds the public terminals and the shared household lines — and those
  // are 5-8% of endpoints carrying fan-out in the hundreds, so they OWN the
  // 99th percentile. Measured on the shipping build: non-ring p99 is 38-50
  // while CONSUMER p99 is 7. Revert the per-person instrument work and keep
  // the terminals, and I.5 still reads ~40 and passes while every real
  // cardholder is back under the two-instrument ceiling. That is
  // `attacker-infra-2026-07`'s "a count of endpoints is not a measurement of
  // the graph" in this file's own newest check.
  //
  // `OwnerType::person` is one roster member's own device, so this is the
  // cards-per-person law read through the router's device assignment and
  // nothing else.
  //
  // THE BAND IS NOT mean - 3.5 SD AND THE REASON IS THAT THE METHOD
  // DEGENERATES HERE. The reading is 7 on all four pairs — SD exactly 0,
  // because the cards-per-person distribution is capped at 7 and the top 1%
  // of consumer devices carry their owner's whole set. A zero-SD band is a
  // bit-equality assert wearing a statistic's clothes, and it would red on
  // any change that moved one person. The floor is a DECLARED margin below a
  // bit-stable reading, sized against the two references that bound it: the
  // pre-round world and the disarm both read exactly 2, and the Fed law's
  // median holding is 3, so 5 separates a working distribution from both.
  // Consumer MAX is printed beside it (11-15) and banded nowhere.
  check(consumerP99 >= kMinConsumerP99Degree,
        std::string(leg.name) +
            ": p99 cards-per-device over CONSUMER (person-owned) devices "
            "must be >= " +
            std::to_string(kMinConsumerP99Degree) + ", got " +
            std::to_string(consumerP99) +
            ". This is the per-person instrument ceiling itself, read off "
            "the corpus; the non-ring form above is satisfied by public "
            "terminals and cannot see it come back");

  // I.6 / I.7 --- MIXED ENDPOINTS, THE PURITY HALF OF THE DEFECT.
  //
  // The half that raising cards-per-person cannot touch. Attacker campaign
  // infrastructure serves only compromise rows, so it was 100% pure fraud —
  // 163 of 163 devices and 103 of 103 on the two legs, and 166/106 on the IP
  // side. With a pure tail, "fan-out >= k" reaches precision 1.0 at whatever
  // k the legitimate population stops at, so moving the legitimate ceiling
  // alone just moves the threshold.
  //
  // Measured pre-fix at 0.262/0.268 on the device axis, every point of it
  // from the victim-endpoint branch. Shipping readings over the four pairs:
  // device 0.6126 0.5659 0.5975 0.5590 (mean 0.5838, SD 0.0255, floor
  // 0.4944); ip 0.6992 0.6551 0.6629 0.6547 (mean 0.6680, SD 0.0212, floor
  // 0.5939). The device floor sits ABOVE the 0.45 direction anchor the
  // research took from LexisNexis, which is corroboration and not the
  // derivation — the constant is the measurement.
  check(mixedDeviceShare >= kMinMixedDeviceShare,
        std::string(leg.name) +
            ": share of fraud rows on a device that ALSO carries legitimate "
            "rows must be >= " +
            std::to_string(kMinMixedDeviceShare) + ", got " +
            std::to_string(mixedDeviceShare) +
            ". A pure-fraud endpoint population is a label wearing a vertex "
            "id, whatever its degree");
  check(mixedIpShare >= kMinMixedIpShare,
        std::string(leg.name) + ": the same on the IP axis must be >= " +
            std::to_string(kMinMixedIpShare) + ", got " +
            std::to_string(mixedIpShare) +
            ". The residential proxy moves rows OFF an attacker address "
            "rather than mixing rows ON to it, so proxy share alone does not "
            "buy this");

  // I.8 --- THE TEMPORAL COMPANION.
  //
  // Nearly as strong as the fan-out rule and computable from the exported
  // `Transaction_Uses_Device.edge_unix_time` alone: 'rows per active day >=
  // 3' scored precision 0.7214 at recall 0.6831 (leg-long) and 0.6229 /
  // 0.6873 (leg-wide) before this round, at 68-79x lift. Its cause is
  // structural — an attacker device is minted with a 120-day tenure but
  // serves cases spanning 6-71 hours, so its rows pile into a handful of
  // days, while a person's device is live continuously.
  //
  // Without this check the round closes one shortcut and hands the model an
  // almost equally good one, and the same symptom is reported against a
  // different feature.
  //
  // Shipping readings over the four pairs: 0.0476 / 0.0509 / 0.0647 /
  // 0.0455, mean 0.0522, SD 0.0086, so the tight ceiling would be 0.0822.
  // IT IS NOT TAKEN, and unlike I.2's the reason is not the estimator: this
  // is a PRECISION, so it scales with the leg's fraud budget, and the four
  // legs share one `targetTxnFraudP` of 0.008. Pinning it near the realized
  // value would make a change to the budget — a knob the file comment
  // already says is deliberately raised — red a check about structure. The
  // scale-free companion is printed beside it: lift 4.90 / 5.41 / 6.70 /
  // 4.96, against 68-79x before this round.
  check(burstPrecision <= kMaxBurstPrecision,
        std::string(leg.name) +
            ": 'device rows per active day >= 3 => fraud' precision must stay "
            "<= " +
            std::to_string(kMaxBurstPrecision) + ", got " +
            std::to_string(burstPrecision) +
            ". Fan-out and burstiness are two views of the same construction; "
            "closing one alone relocates the shortcut");
  check(burstLift > 1.0,
        std::string(leg.name) +
            ": the temporal rule must retain LIFT over base rate (got " +
            std::to_string(burstLift) + "x), for sub-gate C's reason");

  // I.9 --- THE OWNER'S CONSTRAINT: CARD COUNT MUST NOT BE A LABEL.
  //
  // Cards per person is a distribution now, and `exposure.hpp` tilts
  // unauthorized victim selection by ACTIVITY — so more instruments means
  // more activity means more victimisation, which is a live path for an
  // exported vertex COUNT to become a learnable fraud signal. That is the
  // same failure mode sub-gate H forbids for reissue generations and sub-gate
  // G for the merchant register, and it is banded the same way: a lift that
  // must STRADDLE 1.0, plus the whole per-count curve printed so a monotone
  // trend is visible even when the scalar sits in band.
  //
  // IT BUCKETS ON CARDS HELD, WHICH IS WORLD STATE, AND THE PREVIOUS VERSION
  // BUCKETED ON CARDS OBSERVED, WHICH IS THE BUG THIS CHECK EXISTS FOR.
  //
  // The first version keyed on `roll.accounts.size()` — the distinct source
  // accounts a person was SEEN paying from in the card view. That set only
  // fills in as the person transacts, so a busier person has more of their
  // instruments observed AND, through `exposure.hpp`'s activity tilt, more
  // victimisation. The confound the check exists to catch was also choosing
  // the bucket, and it largely cancelled: the scalar read 0.98 whatever the
  // world did. This is `card-churn-2026-07` rule 2 verbatim — "counting
  // generations that appear in the view confounds the measurement with
  // EXPOSURE" — one axis over. `entity::card::Registry::byPerson` is a hash
  // of nothing and an observation of nothing; it is what the person holds.
  //
  // IT IS PERSON-WEIGHTED, WHICH IS ALSO A CORRECTION. The old scalar was
  // row-weighted, so a person with many rows dominated their bucket and the
  // statistic answered "are fraud ROWS concentrated" rather than "are PEOPLE
  // with more cards victimised more". The exported feature is a Party's card
  // degree, so the unit has to be the Party.
  //
  // AND IT IS CHECKED PER BUCKET, TO THE TAIL. An aggregate over ">= 3"
  // averages a monotone trend away; the failure mode is monotone, so every
  // bucket with enough people to speak is banded.
  //
  // WHAT IT READS TODAY, AND THIS IS A NEGATIVE RESULT WORTH RECORDING. The
  // label side is FLAT. Pooled over the four legs (5,400 people, leg-adjusted
  // expectation) the per-person fraud-row lift by cards held reads 0.899 at 0
  // cards — correct and expected, `persona.card.prob` is a factor in the
  // exposure weight — then 1.012 / 1.094 / 1.009 / 0.973 / 0.931 / 0.961 /
  // 0.920 / 1.196 for 1..8. The deviations scatter BOTH directions and none
  // survives the clustered-sampling correction `card-churn-2026-07` rule 3
  // requires (one compromise emits 5-14 rows, so the effective sample is
  // cases, not rows, and the naive SE is ~2.45x too tight).
  //
  // THAT IS ALSO WHAT THE CONSTRUCTION PREDICTS, checked independently
  // against the shipped selection path at N = 400,000: the mean exposure
  // weight by cards held is 1.0245 / 1.0244 / 1.0268 / 1.0249 / 1.0226 /
  // 1.0266 / 1.0232 / 1.0255 for 1..8 against 0.8842 at zero. The mechanism's
  // whole ceiling is 1.027x and it is a STEP AT ADOPTION, not a gradient in
  // count — `cardExposureWeights` reads `rateMultiplier x card.prob x
  // card.share` and has no count term, held count is drawn on an isolated
  // per-person lane with no persona input, and the compromised account is
  // always the victim's PRIMARY. A per-bucket lift materially above 1.0 is
  // therefore not reachable from victim selection at all.
  //
  // SO THE LABEL CHECK IS A TRIPWIRE AND IS LABELLED ONE, exactly as I.3 is:
  // it passes today, and a check that has never failed is indistinguishable
  // from one that cannot. `kDisarmInstrumentExposure` below is its disarm.
  //
  // THE ACTIVITY CHECK IS THE ONE THAT MEASURES SOMETHING LIVE. Card-view
  // rows per person DO rise with cards held — pooled 593 / 608 / 636 / 638 at
  // 1/2/3/4 cards, saturating at `kMaxCreditInstruments`, +5.2% for 2-4 cards
  // and +6.4% for 5+ against the one-card bucket. The mechanism is indirect
  // and is the one quantity in the model that scales linearly with K: each
  // card draws its own `persona.card.limit`, so K cards is K times the
  // revolving headroom; more rows ride credit, the deposit pool drains more
  // slowly, and the liquidity multiplier emits more. It is NOT the label
  // leaking — fraud per person is flat across the same buckets — but it is
  // the channel through which count WOULD become a label if `exposure.hpp`
  // ever adopted the realized-exposure weighting its own header registers as
  // a future refinement. So it is bounded rather than removed: more cards
  // meaning more spend is realistic, and forcing it to exactly 1.0 would be
  // `merchant-selection-2026-08` rule 1 in a new costume.
  {
    const auto &ownership = result.holdings.accounts.ownership;
    const auto &creditCards = result.holdings.creditCards;
    const std::size_t people = ownership.byPersonOffset.empty()
                                   ? 0U
                                   : ownership.byPersonOffset.size() - 1U;

    struct HeldBucket {
      std::size_t people = 0;
      std::size_t rows = 0;
      std::size_t fraudRows = 0;
      /* Sum of SQUARED per-person fraud-row counts, so each bucket can
       * report the standard error of its own mean. See the band note. */
      double fraudSq = 0.0;
    };
    std::map<std::size_t, HeldBucket> byHeld;
    std::size_t allPeople = 0;
    std::size_t allRows = 0;
    std::size_t allFraud = 0;

    for (std::size_t i = 0; i < people; ++i) {
      const auto person = static_cast<pl::entity::PersonId>(i + 1);
      const std::size_t held =
          creditCards.inRange(person) ? creditCards.indicesFor(person).size()
                                      : 0U;
      auto &bucket = byHeld[held];
      ++bucket.people;
      ++allPeople;
      if (const auto it = personRollup.find(person); it != personRollup.end()) {
        /* DISARM for I.9's label check. It passes on the shipping build and
         * the construction says it must, so without this it is
         * indistinguishable from a check that CANNOT fail — sub-gate H's
         * standing argument, and I.3 carries the same switch for the same
         * reason.
         *
         * It cannot be a world switch: victim selection never reads card
         * count, so no dial in the generator produces this coupling. It is
         * therefore an INSTRUMENT disarm — it injects a coupling of a stated
         * size into the measurement and asks whether the check sees it —
         * which also makes it a POWER measurement rather than only a
         * tripwire. That is the more useful artefact here: it converts "this
         * passes" into "this would catch a coupling of at least X per card".
         *
         * MEASURED, and the number is the check's SENSITIVITY FLOOR rather
         * than a pass. At 0.04 — a 4%-per-card tilt — the pooled 4+/1-3
         * ratio reads 1.0830 / 1.1107 / 1.1177 / 1.1407 and reds all four
         * legs. At 0.02 it reads 1.0116 / 1.0345 / 1.0449 / 1.0653 and reds
         * only leg-sizeB. So: the shipped check catches a per-card coupling
         * of 4% everywhere, catches 2% on one leg in four, and is blind
         * below that. A leak smaller than a couple of percent per card would
         * pass this gate, and saying so is the point of running the switch
         * at two levels instead of one. */
        auto fraudRows = it->second.fraudRows;
        if constexpr (kDisarmInstrumentExposure > 0.0) {
          fraudRows = static_cast<std::size_t>(
              static_cast<double>(fraudRows) *
              (1.0 + kDisarmInstrumentExposure * static_cast<double>(held)));
        }
        bucket.rows += it->second.rows;
        bucket.fraudRows += fraudRows;
        const auto f = static_cast<double>(fraudRows);
        bucket.fraudSq += f * f;
        allRows += it->second.rows;
        allFraud += fraudRows;
      }
    }

    const double meanFraudPerPerson =
        allPeople == 0 ? 0.0
                       : static_cast<double>(allFraud) /
                             static_cast<double>(allPeople);

    std::printf("  I.9 cards HELD (world state, person-weighted) over %zu "
                "people:\n",
                allPeople);
    bool everyBucketInBand = true;
    std::size_t worstBucket = 0;
    double worstLift = 1.0;
    for (const auto &[held, bucket] : byHeld) {
      const auto den = static_cast<double>(bucket.people);
      const double rowsPer = den > 0.0 ? static_cast<double>(bucket.rows) / den
                                       : 0.0;
      const double fraudPer =
          den > 0.0 ? static_cast<double>(bucket.fraudRows) / den : 0.0;
      const double lift =
          meanFraudPerPerson > 0.0 ? fraudPer / meanFraudPerPerson : 0.0;
      /* THE BAND IS THE BUCKET'S OWN STANDARD ERROR, NOT A FIXED INTERVAL,
       * AND THAT IS WHAT MAKES A PER-BUCKET CHECK HONEST AT ALL.
       *
       * A fixed [0.80, 1.25] applied per bucket is a FLAKY GATE, and it was
       * measured to be one before this form shipped: it reds on sampling
       * noise in BOTH directions — leg-wide's 70-person 8-card bucket read
       * 1.431 while leg-sizeB's 64-person 7-card bucket read 0.731, on a
       * build whose pooled curve over all four legs is flat and whose
       * construction provably cannot produce a gradient.
       * `card-churn-2026-07` rule 3 is the precedent: sizing a band off an
       * assumed spread rather than the realized one is how a flaky check
       * ships, and there the naive figure was wrong by 2.4x.
       *
       * Fraud rows CLUSTER — one compromise emits 5-14 charges — so no
       * closed-form standard error is right either, and that rule's own
       * remedy (count cases, not rows) is not computable from this rollup.
       * The sample SD of the per-person counts WITHIN the bucket absorbs the
       * clustering exactly, because a person's fraud-row count already IS
       * the per-case sum. So the band is 1 +/- 3.5 * (s / sqrt(n)) / mean —
       * the same 3.5 sigma every other measured band in this file uses,
       * applied to a dispersion the bucket measures for itself instead of
       * one this file declares.
       *
       * That keeps power where power exists: a well-populated bucket gets a
       * tight band (about +/-0.06 at n = 356) and the sparse tail gets a
       * wide one (about +/-0.13 at n = 70), which is the correct answer in
       * both places rather than one compromise between them.
       *
       * `kCardCountLiftLo`/`Hi` survive as the OUTER rail: a bucket whose
       * own SE is wide enough to admit a lift outside them is reporting a
       * coupling large enough to matter whatever its sample size. */
      const double meanF = fraudPer;
      const double variance =
          den > 1.0 ? std::max(0.0, (bucket.fraudSq - den * meanF * meanF) /
                                        (den - 1.0))
                    : 0.0;
      const double stdErr = den > 0.0 ? std::sqrt(variance / den) : 0.0;
      const double halfWidth =
          meanFraudPerPerson > 0.0 ? 3.5 * stdErr / meanFraudPerPerson : 0.0;
      const bool thin = bucket.people < kMinHeldBucketPeople;
      std::printf("      held %2zu: %5zu people, rows/person %8.1f, "
                  "fraudRows/person %7.3f, LIFT %.3fx +/-%.3f%s\n",
                  held, bucket.people, rowsPer, fraudPer, lift, halfWidth,
                  thin ? "  (thin)" : "");
      /* Thin buckets are PRINTED but not banded. The cited Fed count law's
       * tail is genuinely sparse and a 20-person bucket's own SE is wide
       * enough that banding it asserts nothing. */
      if (thin) {
        continue;
      }
      const bool outsideOwnError = std::abs(lift - 1.0) > halfWidth;
      const bool outsideOuterRail =
          lift <= kCardCountLiftLo || lift >= kCardCountLiftHi;
      if (outsideOwnError && outsideOuterRail) {
        everyBucketInBand = false;
        if (std::abs(lift - 1.0) > std::abs(worstLift - 1.0)) {
          worstLift = lift;
          worstBucket = held;
        }
      }
    }

    /* THE ACTIVITY RATIO IS CONDITIONAL ON HOLDING A CARD AT ALL. The 0 -> 1
     * step is ADOPTION, which is `persona.card.prob` and whose realized
     * 0.817-0.833 matches the CITED Fed 0.823 figure; a cardholder spending
     * more than a non-cardholder is the model working. Only the gradient
     * ABOVE one card is this round's to bound. */
    const auto oneCard = byHeld.find(1);
    std::size_t tailPeople = 0;
    std::size_t tailRows = 0;
    /* THE HIGH-POWER FORM, AND IT IS WHY THE PER-BUCKET RAIL ABOVE CAN BE
     * HONEST ABOUT ITS OWN WEAKNESS. Per-person fraud counts are heavily
     * overdispersed at this fraud budget, so a single bucket's SE is +/-0.28
     * even at n = 356 and the rail only catches a gross deviation. Pooling
     * the cardholders into a low half (1-3 cards) and a high half (4+) and
     * comparing their mean fraud rows per person aggregates the whole curve
     * into one comparison of ~500 against ~900 people, which is where the
     * power is. A monotone coupling — the failure mode — moves this even
     * when it hides inside every individual bucket's error bar.
     *
     * It is CONDITIONAL ON HOLDING A CARD for the same reason the activity
     * ratio is: the 0-card bucket differs by ADOPTION, which is cited. */
    std::size_t lowPeople = 0, highPeople = 0;
    std::size_t lowFraud = 0, highFraud = 0;
    for (const auto &[held, bucket] : byHeld) {
      if (held >= 1 && held <= 3) {
        lowPeople += bucket.people;
        lowFraud += bucket.fraudRows;
      } else if (held >= 4) {
        highPeople += bucket.people;
        highFraud += bucket.fraudRows;
      }
    }
    const double lowFraudPer =
        lowPeople > 0 ? static_cast<double>(lowFraud) /
                            static_cast<double>(lowPeople)
                      : 0.0;
    const double highFraudPer =
        highPeople > 0 ? static_cast<double>(highFraud) /
                             static_cast<double>(highPeople)
                       : 0.0;
    const double halfRatio =
        lowFraudPer > 0.0 ? highFraudPer / lowFraudPer : 0.0;
    for (const auto &[held, bucket] : byHeld) {
      if (held >= 2) {
        tailPeople += bucket.people;
        tailRows += bucket.rows;
      }
    }
    const double oneRowsPer =
        (oneCard != byHeld.end() && oneCard->second.people > 0)
            ? static_cast<double>(oneCard->second.rows) /
                  static_cast<double>(oneCard->second.people)
            : 0.0;
    const double tailRowsPer =
        tailPeople > 0 ? static_cast<double>(tailRows) /
                             static_cast<double>(tailPeople)
                       : 0.0;
    const double activityRatio =
        oneRowsPer > 0.0 ? tailRowsPer / oneRowsPer : 0.0;
    std::printf("    activity: rows/person 1 card %.1f vs >=2 cards %.1f, "
                "ratio %.4f (ceiling %.4f)\n",
                oneRowsPer, tailRowsPer, activityRatio, kMaxHeldActivityRatio);
    std::printf("    label   : fraudRows/person 1-3 cards %.3f (%zu people) vs "
                "4+ cards %.3f (%zu people), ratio %.4f (band [%.4f, %.4f])\n",
                lowFraudPer, lowPeople, highFraudPer, highPeople, halfRatio,
                kHeldHalfRatioLo, kHeldHalfRatioHi);

    check(byHeld.size() > 3,
          std::string(leg.name) +
              ": cardholders must reach a distribution of held counts, or "
              "I.9 measures nothing and the instrument ceiling is back");
    check(everyBucketInBand,
          std::string(leg.name) +
              ": 'this Party holds N cards' must carry NO SYSTEMATIC fraud "
              "signal in ANY populated bucket — bucket " +
              std::to_string(worstBucket) + " read lift " +
              std::to_string(worstLift) + ", outside [" +
              std::to_string(kCardCountLiftLo) + ", " +
              std::to_string(kCardCountLiftHi) +
              "]. Card degree is an exported Party feature; if victim "
              "selection's activity tilt turns it into a label, this round "
              "has closed a degree shortcut by opening a cardinality one");
    check(halfRatio > kHeldHalfRatioLo && halfRatio < kHeldHalfRatioHi,
          std::string(leg.name) +
              ": pooled over cardholders, mean fraud rows per person for 4+ "
              "card holders over 1-3 card holders must straddle 1.0 — got " +
              std::to_string(halfRatio) + ", band [" +
              std::to_string(kHeldHalfRatioLo) + ", " +
              std::to_string(kHeldHalfRatioHi) +
              "]. This is the form with the power; a monotone coupling moves "
              "it while hiding inside every single bucket's error bar");
    check(activityRatio > 0.0 && activityRatio <= kMaxHeldActivityRatio,
          std::string(leg.name) +
              ": card-view rows per person must not scale with cards HELD "
              "beyond " +
              std::to_string(kMaxHeldActivityRatio) + "x, got " +
              std::to_string(activityRatio) +
              ". Aggregate credit headroom is the only quantity in the model "
              "that grows linearly with card count, and activity is what "
              "`exposure.hpp` would tilt on if it ever moved to realized "
              "exposure");
  }
}

/* SUB-GATE J — THE PUBLIC-ENDPOINT REACH CEILING, DRIVEN AT PRODUCTION
 * POPULATION RATHER THAN INFERRED FROM A GATE LEG.
 *
 * Sub-gate I bands the joint distribution of degree and label on a corpus, and
 * it cannot see this: a head that swallows a quarter of the roster passes I.2
 * comfortably as long as the rows it carries are mostly legitimate. What was
 * wrong with the first public pool was not its label mix but its SIZE.
 *
 * The first ceiling was written on the drawn WEIGHT (`kMaxReachMultiple = 40`)
 * and a weight is only ever read relative to the pool's total, so the user
 * count it produced was `peoplePerLine * w / E[w]` — a HIDDEN cap of about
 * 1,008 users at the mean of 64 that pool started with, which no constant
 * stated and which moved whenever the mean did. Measured on the corpus legs
 * it put 17.4% (900 people) and 25.3% (1,800) of every card account in the
 * world onto ONE endpoint. The largest real browser-fingerprint anonymity set
 * is 1,394 of 1,816,776 desktop fingerprints — 0.077% of its own population —
 * and 13,241 of 251,166 mobile, 5.27% (Gomez-Boix, Laperdrix and Baudry,
 * "Hiding in the Crowd", WWW 2018, accessed 2026-08-07). The absolute counts
 * were physically reachable; the SHARE was not.
 *
 * SO THE LAW IS STATED IN USERS AND THIS SUB-GATE CHECKS IT IN USERS. That is
 * `merchant-selection` rule 1 one layer over — reach is not volume — and it is
 * why cards per endpoint is PRINTED on the corpus legs and banded nowhere: it
 * is the product of this law and the cards-per-person law, which moved in this
 * same round, so a band on it would be a band on a superseded construction.
 *
 * AND IT RUNS AT POPULATION 500,000 DIRECTLY, WHICH IS `merchant-selection`
 * RULE 8's PATTERN. Head SHARE is emergent from lines-per-roster and is 13.3%
 * at pop 900 for a reason no ceiling can remove — 38 lines cannot divide a
 * roster more finely than that — so banding share at a gate leg would pin a
 * regime production never runs. The sampler is draw-free of any ledger and
 * costs milliseconds, so the band goes where the claim is: at the target
 * population. The gate-leg rows are PRINTED beside it.
 *
 * A ROSTER-SHARE LEG WAS CONSIDERED FOR THE CEILING ITSELF AND REJECTED ON
 * MEASUREMENT. `min(120 users, 0.04 * roster)` reads well and is incoherent at
 * the bottom: 0.04 * 300 is 12 users against a MEAN of 24, so the cap would
 * sit below the mean, drive the flattening exponent to 0 and return a pool
 * where every terminal serves the same number of people — replacing the tail
 * this population exists to create with the flat block it exists to remove. It
 * binds only below about 3,000 people, which is to say only on gate legs.
 *
 * THE TWO POOLS ARE DELIBERATELY DIFFERENT SHAPES AND ONLY ONE IS BANDED ON
 * ITS TAIL. A public terminal's user count is BEHAVIOURAL and spans the office
 * box at low tens through the library workstation at low hundreds, so its tail
 * is the point. A carrier NAT's is an ENGINEERING PARAMETER: an ISP allocates
 * a fixed port chunk, which is why Richter et al. report two near-deterministic
 * operating points (64 subscribers per public IPv4 at a 1K chunk, ~128 at 512)
 * rather than a distribution. Measured p90/p50 users 1.84 for terminals against
 * 1.11 for carrier addresses — that narrowness is the source speaking, not a
 * flattened law, and banding the NAT pool to the terminal's floor would be
 * asserting a spread nothing reports. */
struct ReachReading {
  double gamma = 1.0;
  double topShare = 0.0;
  double topUsers = 0.0;
  double tailRatio = 0.0;
  std::size_t lines = 0;
};

/* Both synth units floor their line count at 2 so a world smaller than one
 * endpoint still has endpoints in it. Restated rather than exposed because it
 * cannot bind at any population driven here — the smallest is 900 people over
 * a mean of 24, which sizes to 38 lines. */
constexpr std::size_t kMinPublicLines = 2;

// The largest REAL anonymity set as a share of its own population: 1,394
// desktop fingerprints out of 1,816,776 (Gomez-Boix et al.). The mobile
// figure is 5.27% and is deliberately NOT used — it is a fingerprint
// COLLISION class, and this pool models SHARING. See the registration in
// `entities/infra/public_endpoints.hpp`.
constexpr double kMaxHeadRosterShare = 0.00077;

// p90/p50 users across terminal lines, at production population. Six seeds at
// each of pop 8,000 / 50,000 / 500,000: 1.800 +- 0.105, 1.816 +- 0.047, 1.835
// +- 0.009. The floor is the lowest mean less 3.5x the largest SD, 1.433,
// rounded down. A pool whose ceiling has driven the exponent to zero scores
// exactly 1.000, so the floor separates the two by a wide margin.
constexpr double kMinTerminalTailRatio = 1.40;

// The ceiling is solved by a fixed 60-iteration bisection, so it lands on the
// target to far better than this; the tolerance exists so the check reads as
// "binds" rather than as a float comparison.
constexpr double kReachCeilingTolerance = 1e-6;

constexpr std::uint32_t kProductionPeople = 500000;

/* DISARM SWITCH, left in the file for the reason sub-gate H's is: a negative
 * that has never been made to fail is indistinguishable from a check that
 * cannot. Setting this true restores the pre-round construction EXACTLY — the
 * terminal mean of 64 that came from Richter's subscribers-per-public-IPv4,
 * and a ceiling far enough away that the drawn weight ratio is what governs
 * the head again.
 *
 * MEASURED WITH IT ON, four seeds at pop 500,000: head 943.09 users at a
 * roster share of 0.1886% against a band of 0.077%, and the ceiling never
 * binds at any population. FIVE checks red — J.3 at 8,000, 50,000 and 500,000
 * and J.4 and J.6 at production. The realized head reproduces the derived
 * hidden cap of `64 * 40 / 2.54 = 1,008` to within its own sampling spread,
 * which is the evidence that the weight ratio was the thing setting the head
 * rather than a coincidence of the seeds.
 *
 * J.6 REDDING IS THE ONE THAT WAS NOT PREDICTED AND IT IS THE MOST USEFUL:
 * uncapped, the head reads 978.02 users at 365 days against 945.04 at 3,652.
 * The reach weights are drawn once per line either way, but the chain between
 * them is not, so which uniforms the reach draws land on moves with the
 * window — and an unbound head passes that movement straight through. The
 * ceiling is what makes the head a property of the roster alone. */
constexpr bool kDisarmReachCeiling = false;
constexpr double kDisarmTerminalMean = 64.0;
constexpr double kDisarmCeiling = 1.0e6;

template <typename Endpoint>
[[nodiscard]] ReachReading
summarizeReach(const pl::infra::PublicEndpointPool<Endpoint> &pool,
               std::uint32_t people) {
  ReachReading out;
  out.gamma = pool.reachGamma;
  out.topShare = pool.maxLineShare;
  out.topUsers = pool.maxLineShare * static_cast<double>(people);
  out.lines = pool.lines.size();

  std::vector<double> shares;
  shares.reserve(pool.reachCdf.size());
  double previous = 0.0;
  for (const auto cumulative : pool.reachCdf) {
    shares.push_back(cumulative - previous);
    previous = cumulative;
  }
  if (shares.empty()) {
    return out;
  }
  std::sort(shares.begin(), shares.end());
  const auto quantile = [&](double p) {
    const auto idx =
        static_cast<std::size_t>(p * static_cast<double>(shares.size() - 1));
    return shares[idx];
  };
  const auto median = quantile(0.50);
  out.tailRatio = median > 0.0 ? quantile(0.90) / median : 0.0;
  return out;
}

[[nodiscard]] ReachReading
terminalReach(std::uint64_t seed, std::uint32_t people, std::int32_t days) {
  // Read off the SHIPPING rules rather than restated, so a level that moves in
  // the header moves here too. The precondition has to measure the production
  // function, which is `burst-rate-2026-07` rule 2.
  const pl::synth::infra::devices::AssignmentRules rules;
  const auto meanUsers =
      kDisarmReachCeiling ? kDisarmTerminalMean : rules.peoplePerTerminal;
  const auto ceiling =
      kDisarmReachCeiling ? kDisarmCeiling : rules.maxUsersPerTerminal;
  const pl::random::RngFactory laneFactory{seed};
  auto rng = laneFactory.rng({"infra", "devices", "terminals"});
  const pl::time::Window window{.start = pl::time::makeTime({2012, 1, 1}),
                                .days = days};
  const auto pool =
      pl::synth::infra::publics::buildPublicPool<pl::devices::Identity>(
          rng, window,
          pl::synth::infra::publics::PoolSpec{
              .lineCount = pl::synth::infra::publics::lineCountFor(
                  people, meanUsers, kMinPublicLines),
              .people = people,
              .maxUsersPerLine = ceiling,
              .tenureDays =
                  pl::synth::infra::tenure::publicTerminalTenureDays(),
              .rowShare = rules.terminalRowShare,
              .poolDomain = pl::infra::publicEndpoints::kTerminalPoolDomain,
          },
          [](std::size_t line, std::size_t link) {
            return pl::devices::Identity::publicTerminal(
                pl::infra::kPublicTerminalOwnerIdBase +
                    static_cast<std::uint64_t>(line),
                static_cast<std::uint32_t>(link));
          });
  return summarizeReach(pool, people);
}

[[nodiscard]] ReachReading
carrierNatReach(std::uint64_t seed, std::uint32_t people, std::int32_t days) {
  const pl::synth::infra::ips::AssignmentRules rules;
  const pl::random::RngFactory laneFactory{seed};
  auto rng = laneFactory.rng({"infra", "ips", "carrier_nat"});
  const pl::time::Window window{.start = pl::time::makeTime({2012, 1, 1}),
                                .days = days};
  // The real mint, because it SPENDS A UNIFORM. A draw-free stand-in would
  // shift every reach draw after the first line and this sub-gate would be
  // reading a law the generator does not use.
  const auto pool =
      pl::synth::infra::publics::buildPublicPool<pl::network::Ipv4>(
          rng, window,
          pl::synth::infra::publics::PoolSpec{
              .lineCount = pl::synth::infra::publics::lineCountFor(
                  people, rules.peoplePerCarrierNat, kMinPublicLines),
              .people = people,
              .maxUsersPerLine = rules.maxUsersPerCarrierNat,
              .tenureDays = pl::synth::infra::tenure::carrierNatTenureDays(),
              .rowShare = rules.carrierNatRowShare,
              .poolDomain = pl::infra::publicEndpoints::kCarrierNatPoolDomain,
          },
          [&rng](std::size_t, std::size_t) {
            return pl::network::randomIpv4(rng);
          });
  return summarizeReach(pool, people);
}

void measurePublicReach() {
  const pl::synth::infra::devices::AssignmentRules deviceRules;
  const pl::synth::infra::ips::AssignmentRules ipRules;

  std::printf("\n===== J public-endpoint reach ceiling (sampler driven "
              "directly, no ledger) =====\n");
  std::printf("  terminal  mean %.0f users/line, ceiling %.0f\n",
              deviceRules.peoplePerTerminal, deviceRules.maxUsersPerTerminal);
  std::printf("  carrier   mean %.0f users/line, ceiling %.0f\n",
              ipRules.peoplePerCarrierNat, ipRules.maxUsersPerCarrierNat);

  struct Point {
    const char *label;
    std::uint32_t people;
    bool banded;
  };
  // 900 is a corpus leg and is PRINTED ONLY. Its head share cannot clear the
  // production band and no ceiling can make it: 38 lines is as finely as 900
  // people divide.
  const Point points[] = {
      {"leg", 900U, false},
      {"mid", 8000U, true},
      {"mid", 50000U, true},
      {"prod", kProductionPeople, true},
  };
  constexpr std::uint64_t kSeeds[] = {20260728ULL, 20268647ULL, 20276566ULL,
                                      20284485ULL};

  for (const auto &point : points) {
    for (const auto pool : {0, 1}) {
      const char *name = pool == 0 ? "terminal" : "carrier ";
      const double ceiling =
          pool == 0 ? (kDisarmReachCeiling ? kDisarmCeiling
                                           : deviceRules.maxUsersPerTerminal)
                    : ipRules.maxUsersPerCarrierNat;
      double sumUsers = 0.0;
      double sumShare = 0.0;
      double sumRatio = 0.0;
      double worstUsers = 0.0;
      std::size_t lines = 0;
      for (const auto seed : kSeeds) {
        const auto reading = pool == 0
                                 ? terminalReach(seed, point.people, 1461)
                                 : carrierNatReach(seed, point.people, 1461);
        sumUsers += reading.topUsers;
        sumShare += reading.topShare;
        sumRatio += reading.tailRatio;
        worstUsers = std::max(worstUsers, reading.topUsers);
        lines = reading.lines;

        // J.1 --- THE CEILING IS NEVER EXCEEDED. Construction identity, not a
        // band. The pre-round build reads 945 users at pop 500,000 against a
        // ceiling nothing declared.
        check(reading.topUsers <= ceiling + kReachCeilingTolerance,
              std::string(name) + " pop " + std::to_string(point.people) +
                  ": the head line's expected user count must stay at or "
                  "below the declared ceiling " +
                  std::to_string(ceiling) + ", got " +
                  std::to_string(reading.topUsers));

        // J.2 --- THE CEILING IS A CAP, NEVER A PIN.
        //
        // A pool whose drawn head already sits under the ceiling must come
        // back at the identity exponent and bit-identical to one built with no
        // ceiling at all — that is what lets the cap be disarmed without the
        // disarm itself moving a weight. The converse is the other half: an
        // exponent below 1 may only ever be the ceiling binding.
        if (reading.gamma >= 1.0) {
          check(reading.topUsers <= ceiling + kReachCeilingTolerance,
                std::string(name) + " pop " + std::to_string(point.people) +
                    ": an untouched pool cannot sit above its own ceiling");
        } else {
          check(std::fabs(reading.topUsers - ceiling) <= kReachCeilingTolerance,
                std::string(name) + " pop " + std::to_string(point.people) +
                    ": a flattened pool must land ON the ceiling, got " +
                    std::to_string(reading.topUsers) +
                    ". An exponent below 1 that misses the target means the "
                    "bisection is solving something other than the head");
        }
      }

      const auto seeds = static_cast<double>(std::size(kSeeds));
      std::printf("  J %-8s %-4s pop %7u: %6zu lines, head %7.2f users "
                  "(%.4f%% of roster), p90/p50 %.3f\n",
                  name, point.label, point.people, lines, sumUsers / seeds,
                  100.0 * sumShare / seeds, sumRatio / seeds);

      if (!point.banded) {
        continue;
      }

      // J.3 --- THE CEILING BINDS, WHICH IS WHAT MAKES THIS A USERS LAW.
      //
      // Without it the share check below passes on the mean alone: dropping
      // the terminal mean to 24 with NO ceiling still reads 0.071% at pop
      // 500,000, just under the band. What the ceiling buys is that the head
      // is the SAME 120 users at 8,000, 50,000 and 500,000 people instead of
      // drifting with the pool size — a population-invariant quantity, which
      // is the only kind a source can bound.
      check(std::fabs(worstUsers - ceiling) <= kReachCeilingTolerance,
            std::string(name) + " pop " + std::to_string(point.people) +
                ": the users ceiling must BIND at production scale, got a "
                "head of " +
                std::to_string(worstUsers) + " against " +
                std::to_string(ceiling) +
                ". A ceiling that never binds is not a law, and the quantity "
                "left governing the head is the drawn weight ratio");

      if (point.people == kProductionPeople) {
        // J.4 --- AND THE SHARE IT PRODUCES IS SMALLER THAN THE LARGEST REAL
        // ONE. This is the check that reds the pre-round construction: mean
        // 64 with the weight-ratio cap reads 0.189% here, and the carrier
        // pool uncapped reads 0.282%.
        check(sumShare / seeds <= kMaxHeadRosterShare,
              std::string(name) +
                  ": at production population the head endpoint's share of "
                  "the roster must stay at or below " +
                  std::to_string(kMaxHeadRosterShare) + ", got " +
                  std::to_string(sumShare / seeds) +
                  ". The absolute count is physically reachable; the share is "
                  "what no real anonymity set comes near");

        // J.5 --- AND THE TERMINAL TAIL SURVIVED BEING CAPPED.
        //
        // Terminals only. A carrier NAT's subscriber count is a port-chunk
        // allocation and is near-deterministic by design; see the note above.
        if (pool == 0) {
          check(sumRatio / seeds >= kMinTerminalTailRatio,
                std::string(name) +
                    ": p90/p50 users across terminal lines must stay >= " +
                    std::to_string(kMinTerminalTailRatio) + ", got " +
                    std::to_string(sumRatio / seeds) +
                    ". A ceiling low enough to flatten the pool replaces one "
                    "cliff with another — a block of identical endpoints and "
                    "nothing between them and the household population");
        }
      }
    }
  }

  // J.6 --- THE LAW IS WINDOW-INDEPENDENT, AND ONE HORIZON CANNOT SHOW IT.
  //
  // `burst-rate-2026-07` rule 1 in its exact original form: a per-roster
  // quantity and a per-window one are indistinguishable at a single window
  // length. The pool is sized off the roster and its reach weights are drawn
  // once per line, so a longer window may add replacement LINKS and may never
  // move the head's user count.
  for (const auto pool : {0, 1}) {
    const char *name = pool == 0 ? "terminal" : "carrier ";
    const auto shortRun =
        pool == 0 ? terminalReach(kSeeds[0], kProductionPeople, 365)
                  : carrierNatReach(kSeeds[0], kProductionPeople, 365);
    const auto longRun =
        pool == 0 ? terminalReach(kSeeds[0], kProductionPeople, 3652)
                  : carrierNatReach(kSeeds[0], kProductionPeople, 3652);
    std::printf("  J %-8s window invariance: 365d head %7.2f, 3652d head "
                "%7.2f users\n",
                name, shortRun.topUsers, longRun.topUsers);
    check(std::fabs(shortRun.topUsers - longRun.topUsers) <=
              kReachCeilingTolerance,
          std::string(name) +
              ": the head's user count must not move with window length, got " +
              std::to_string(shortRun.topUsers) + " at 365d against " +
              std::to_string(longRun.topUsers) +
              " at 3652d. A reach that grows with the window is a "
              "window-length-dependent constant wearing a population law's "
              "name");
    check(shortRun.lines == longRun.lines,
          std::string(name) +
              ": the line count is sized off the ROSTER and must not move "
              "with the window");
  }
}

} // namespace

int main() {
  const Leg legs[] = {
      // Long and narrow: campaigns succeed each other many times over,
      // so the replacement chains and the coverage floor are exercised.
      {"leg-long", 20260728ULL, 2012, 1461, 900, 0.008},
      // Short and wide: the sizing rule claims mean concurrent operators
      // is population-driven and window-independent. Two shapes is the
      // cheapest test of that claim.
      {"leg-wide", 20260729ULL, 2016, 731, 1800, 0.008},
      {"leg-sizeA", 20260730ULL, 2014, 1096, 1200, 0.008},
      {"leg-sizeB", 20260731ULL, 2018, 900, 1500, 0.008},
  };

  try {
    // Once, not per leg. It drives the sampler alone and its whole point is
    // that the population it reads is NOT a leg's.
    measurePublicReach();
    for (const auto &leg : legs) {
      const auto poolSet = buildPoolSet(leg.seed);
      const auto result = runLeg(poolSet, leg);
      measure(leg, result);
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "\n%d check(s) failed.\n", g_failures);
    return 1;
  }
  std::printf("\nAll endpoint-graph checks passed.\n");
  return 0;
}
