//
// tests/test_card_enumeration.cpp
//
// CARD TESTING (BIN enumeration) — the fan-out that must NOT predict fraud.
//
// WHY THIS GATE EXISTS
// `attacker-infra-2026-07` closed a defect where no attacker endpoint was ever
// seen by two victims, so device-to-card fan-out was zero by construction.
// Closing it opened the opposite hazard: if the ONLY source of high fan-out is
// an attacker running compromises, then degree alone separates the label and a
// GNN learns "this device touches many cards" instead of learning the graph.
//
// `infra/enumeration.hpp` adds the legitimate high-fan-out population — an
// operator sweeping a stolen card list with one small authorization each. Every
// probe row is DECLINED with its label WITHHELD, so those edges carry no
// supervision.
//
// WHAT IS ACTUALLY CHECKED, and the middle one is the point of the file:
//
//   A  the mechanism produces probes at all, and they concentrate on few
//      endpoints rather than spreading one-per-device
//   B  a probed card is NOT more likely to be a fraud victim — the lift must
//      STRADDLE 1.0, with the sign free to flip across seeds
//   C  probe endpoints reach cards that see no fraud, so "high-degree device"
//      cannot be read as "compromise device"
//
// B IS NOT A FORMALITY. Selection is a hash of the card key alone, so the null
// is EXACT and the only way the lift moves is if something correlated leaks in
// — which is exactly what would happen if a future edit keyed the probe on
// activity, on the owner, or on anything the victim sampler also reads
// (`exposure.hpp` tilts victim selection on activity). CLAUDE.md
// merchant-ownership sub-gate G is the same check for the same reason.
//
// THE BANDS ARE MEASURED, NOT DERIVED. A lift band derived analytically at
// exactly 1.0 would be vacuous under sampling noise — CLAUDE.md
// merchant-selection rule 6, and the third time that trap has been recorded.
//
// HARD-ENFORCED.
//

#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/enumeration.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

namespace pl = PhantomLedger;

constexpr auto kCardTag = pl::channels::tag(pl::channels::Legit::cardPurchase);
constexpr auto kMerchantTag = pl::channels::tag(pl::channels::Legit::merchant);

/* Cards per probe endpoint. MEASURED 7.3 / 19.5 / 9.6 / 11.6, so the floor is
 * not a mean-minus-sigma band — it is a floor against DEGENERACY. One probe per
 * endpoint scores exactly 1.0 and is the same graph as no enumeration at all,
 * which is the failure this guards: a future edit that spread probes over the
 * whole endpoint inventory would leave the row count intact and destroy the
 * property. Set well below the observed minimum on purpose; the level itself
 * moves with campaign count and is PRINTED, not banded. */
constexpr double kMinMeanCardsPerProbeDevice = 3.0;

/* THE ANTI-LEAK BAND. Per-leg fraud lift on "this card was probed".
 *
 * MEASURED: 1.0968 / 0.9990 / 0.7791 / 1.0671, mean 0.9855, sample SD 0.1436,
 * mean +/- 3.5 SD = [0.4830, 1.4880]. Rounded outward.
 *
 * IT STRADDLES 1.0 AND THE SIGN FLIPS ACROSS SEEDS, which is the evidence
 * there is no construction correlation — the same reading merchant-ownership
 * sub-gate G takes, and a stronger statement than any one-sided bound.
 *
 * THE BAND IS WIDE BECAUSE THE POWER IS LOW, and saying so is the point. Only
 * ~180 cards are probed per leg against a ~14% base rate, giving a binomial
 * relative SE near 0.19 — larger than the 0.144 actually observed, so the
 * spread is sampling noise and not a residual signal. A tight band here would
 * be flaky, not strict. The POOLED form below is where the power is. */
constexpr double kLiftLo = 0.48;
constexpr double kLiftHi = 1.49;

/* THE POOLED LIFT, over all four legs at once: 814 probed cards against
 * 19,025 unprobed. MEASURED 0.9876.
 *
 * BANDED FROM THE ANALYTIC LOG-RATIO SE (0.0905), at 3.5 SE, because four legs
 * give four readings of the per-leg statistic and only ONE of the pooled one —
 * there is nothing to take a sample SD over. CLAUDE.md card-churn rule 3 warns
 * that a binomial band can be badly wrong when the underlying events CLUSTER,
 * so this is cross-checked rather than trusted: the unit here is a CARD, not a
 * row, and the observed per-leg SD (0.144) comes in BELOW its own binomial
 * prediction (0.19) at a ratio of 0.76. The binomial is conservative in this
 * form, which is the safe direction. Re-measure if the unit ever changes. */
constexpr double kPooledLiftLo = 0.72;
constexpr double kPooledLiftHi = 1.36;

/* Share of the cards a probe endpoint touched that NEVER saw fraud.
 *
 * MEASURED: 0.8116 / 0.8791 / 0.8907 / 0.8591, mean 0.8601, sample SD 0.0353,
 * mean - 3.5 SD = 0.7366. Rounded down.
 *
 * This is the check that makes the round's claim concrete. A high-degree
 * endpoint is only uninformative if most of what it reaches is CLEAN; an
 * endpoint touching 60 cards of which 55 are victims is still a compromise
 * tell, whatever the row labels say. Disarm reads 0.1370-0.1940. */
constexpr double kMinCleanShare = 0.73;

int failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

struct Leg {
  const char *name;
  std::uint64_t seed;
  int startYear;
  std::int32_t days;
  std::int32_t population;
};

[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  const pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

/* Same construction as test_card_endpoint_graph's leg runner, and for the same
 * reasons: the ring rate is raised so the world carries AML rings at this
 * population, and the fraud budget so operators carry enough cases for the
 * endpoint layer to be non-degenerate. Neither touches the mechanism here. */
[[nodiscard]] pl::pipeline::SimulationResult
runLeg(const pl::synth::pii::PoolSet &poolSet, const Leg &leg) {
  const pl::time::Window window{
      .start = pl::time::makeTime({leg.startYear, 1, 1}),
      .days = leg.days,
  };

  pl::synth::people::Fraud fraudProfile{};
  fraudProfile.rings.perTenKMean = 20.0;
  fraudProfile.rings.perTenKSigma = 0.0;
  fraudProfile.limits.targetTxnFraudP = 0.008;

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

// ------------------------------------------------------- the measurement

struct CardFacts {
  std::int64_t firstTs = 0;
  bool fraud = false;
};

struct LegMeasurement {
  std::size_t cards = 0;
  std::size_t probed = 0;

  std::size_t probeDevices = 0;
  std::size_t maxCardsPerDevice = 0;
  double meanCardsPerDevice = 0.0;

  double fraudRateProbed = 0.0;
  double fraudRateUnprobed = 0.0;
  double lift = 0.0;

  /* Raw counts, kept so the four legs can be POOLED. Ratios cannot be
   * averaged into a pooled ratio; the counts have to survive. */
  std::size_t fraudProbedCount = 0;
  std::size_t fraudUnprobedCount = 0;
  std::size_t unprobedCount = 0;

  /* Of the cards a probe device touched, how many never saw fraud. The
   * quantity that makes a high-degree device unreadable as a compromise
   * device. */
  double cleanShareOnProbeDevices = 0.0;
};

/* `disarmLabelAware` drives the leak this gate exists to catch: it selects
 * probes on the card's FRAUD STATUS instead of on its key. A gate that has
 * never been made to fail is indistinguishable from one that cannot.
 * Probability matched to the armed rate so only the CORRELATION differs. */
[[nodiscard]] LegMeasurement
measure(const pl::pipeline::SimulationResult &result, bool disarmLabelAware) {
  const auto &attackers = result.infra.attackers;

  /* The card view: the same channel filter the exporter applies, so "card" here
   * means what a card row means downstream. */
  std::map<pl::entity::Key, CardFacts> cards;
  for (const auto &tx : result.transfers.ledger.posted.txns) {
    const auto channel = tx.session.channel;
    if (channel != kCardTag && channel != kMerchantTag) {
      continue;
    }
    auto [it, inserted] = cards.try_emplace(tx.source, CardFacts{});
    if (inserted) {
      it->second.firstTs = tx.timestamp;
    }
    it->second.fraud = it->second.fraud || tx.fraud.flag != 0;
  }

  LegMeasurement out;
  out.cards = cards.size();

  std::map<pl::devices::Identity, std::set<pl::entity::Key>> byDevice;
  std::size_t fraudProbed = 0;
  std::size_t fraudUnprobed = 0;
  std::size_t unprobed = 0;

  for (const auto &[key, facts] : cards) {
    auto probe =
        pl::infra::enumeration::probeFor(attackers, key, facts.firstTs);

    if (disarmLabelAware) {
      /* Keep the endpoint resolution, replace only the SELECTION: fraud cards
       * are probed, clean cards are not, at a rate near the armed one. */
      const bool select =
          facts.fraud || (pl::infra::derived::splitmix(key.number) % 10'000U <
                          pl::infra::enumeration::kProbedBasisPoints / 2);
      if (!select) {
        probe.reset();
      } else if (!probe.has_value()) {
        probe = pl::infra::enumeration::probeFor(
            attackers, pl::entity::Key{key.role, key.bank, key.number ^ 1ULL},
            facts.firstTs);
      }
    }

    if (probe.has_value()) {
      ++out.probed;
      byDevice[probe->device].insert(key);
      if (facts.fraud) {
        ++fraudProbed;
      }
    } else {
      ++unprobed;
      if (facts.fraud) {
        ++fraudUnprobed;
      }
    }
  }

  out.probeDevices = byDevice.size();
  std::size_t totalTouched = 0;
  std::size_t cleanTouched = 0;
  for (const auto &[device, touched] : byDevice) {
    out.maxCardsPerDevice = std::max(out.maxCardsPerDevice, touched.size());
    totalTouched += touched.size();
    for (const auto &key : touched) {
      if (!cards.at(key).fraud) {
        ++cleanTouched;
      }
    }
  }
  out.meanCardsPerDevice =
      out.probeDevices == 0 ? 0.0
                            : static_cast<double>(totalTouched) /
                                  static_cast<double>(out.probeDevices);
  out.cleanShareOnProbeDevices =
      totalTouched == 0 ? 0.0
                        : static_cast<double>(cleanTouched) /
                              static_cast<double>(totalTouched);

  out.fraudRateProbed = out.probed == 0
                            ? 0.0
                            : static_cast<double>(fraudProbed) /
                                  static_cast<double>(out.probed);
  out.fraudRateUnprobed = unprobed == 0 ? 0.0
                                        : static_cast<double>(fraudUnprobed) /
                                              static_cast<double>(unprobed);
  out.lift = out.fraudRateUnprobed > 0.0
                 ? out.fraudRateProbed / out.fraudRateUnprobed
                 : 0.0;
  out.fraudProbedCount = fraudProbed;
  out.fraudUnprobedCount = fraudUnprobed;
  out.unprobedCount = unprobed;
  return out;
}

void report(const char *name, const LegMeasurement &m) {
  std::printf("  %-14s cards %5zu  probed %4zu (%.2f%%)  devices %3zu  "
              "mean/dev %5.1f  max %4zu\n",
              name, m.cards, m.probed,
              m.cards == 0 ? 0.0
                           : 100.0 * static_cast<double>(m.probed) /
                                 static_cast<double>(m.cards),
              m.probeDevices, m.meanCardsPerDevice, m.maxCardsPerDevice);
  std::printf("  %-14s fraud|probed %.4f  fraud|unprobed %.4f  LIFT %.4f  "
              "clean-on-probe-devices %.4f\n",
              "", m.fraudRateProbed, m.fraudRateUnprobed, m.lift,
              m.cleanShareOnProbeDevices);
  std::fflush(stdout);
}

} // namespace

int main() {
  const bool disarm = std::getenv("PL_ENUM_DISARM") != nullptr;

  std::printf("=== card-testing (BIN enumeration) ===\n");
  if (disarm) {
    std::printf("  PL_ENUM_DISARM: probe selection reads the FRAUD LABEL. "
                "Sub-gate B must go RED.\n");
  }

  const Leg legs[] = {
      {"leg-long", 20260728, 2012, 1461, 900},
      {"leg-wide", 20260729, 2016, 731, 1800},
      {"leg-sizeA", 20260730, 2014, 1096, 1200},
      {"leg-sizeB", 20260731, 2015, 900, 1500},
  };

  std::vector<LegMeasurement> all;
  for (const auto &leg : legs) {
    const auto poolSet = buildPoolSet(leg.seed);
    const auto result = runLeg(poolSet, leg);
    const auto m = measure(result, disarm);
    report(leg.name, m);
    all.push_back(m);
  }

  std::printf("\n");

  // ---------------------------------------------- A: the mechanism runs
  for (std::size_t i = 0; i < all.size(); ++i) {
    const auto &m = all[i];
    const std::string tag{legs[i].name};

    check(m.probed > 0,
          tag + ": NO card was probed, so every check below is vacuous");
    check(m.probeDevices > 0, tag + ": probes resolved to no endpoint");

    /* CONCENTRATION IS THE PROPERTY, NOT COUNT. One probe per device would be
     * the same graph as no enumeration at all — it is the SHARED endpoint that
     * makes fan-out stop separating the label. */
    check(m.meanCardsPerDevice >= kMinMeanCardsPerProbeDevice,
          tag + ": probes spread too thin — mean " +
              std::to_string(m.meanCardsPerDevice) + " cards per endpoint, "
              "floor " + std::to_string(kMinMeanCardsPerProbeDevice));
  }

  // -------------------------------------- B: the lift must straddle 1.0
  for (std::size_t i = 0; i < all.size(); ++i) {
    const auto &m = all[i];
    const std::string tag{legs[i].name};
    check(m.lift >= kLiftLo && m.lift <= kLiftHi,
          tag + ": a probed card predicts fraud — lift " +
              std::to_string(m.lift) + " outside the measured band [" +
              std::to_string(kLiftLo) + ", " + std::to_string(kLiftHi) +
              "]. Probe selection has become correlated with the label.");
  }

  /* THE POOLED FORM, where the power is. Four legs' counts summed, not four
   * ratios averaged. */
  {
    std::size_t probed = 0;
    std::size_t fraudProbed = 0;
    std::size_t unprobed = 0;
    std::size_t fraudUnprobed = 0;
    for (const auto &m : all) {
      probed += m.probed;
      fraudProbed += m.fraudProbedCount;
      unprobed += m.unprobedCount;
      fraudUnprobed += m.fraudUnprobedCount;
    }
    const double pProbed =
        probed == 0 ? 0.0
                    : static_cast<double>(fraudProbed) /
                          static_cast<double>(probed);
    const double pUnprobed =
        unprobed == 0 ? 0.0
                      : static_cast<double>(fraudUnprobed) /
                            static_cast<double>(unprobed);
    const double pooled = pUnprobed > 0.0 ? pProbed / pUnprobed : 0.0;

    std::printf("  POOLED  probed %zu (fraud %zu, %.4f)  unprobed %zu "
                "(fraud %zu, %.4f)  LIFT %.4f\n",
                probed, fraudProbed, pProbed, unprobed, fraudUnprobed,
                pUnprobed, pooled);
    std::fflush(stdout);

    check(probed > 0 && unprobed > 0,
          "pooled: one side of the comparison is empty");
    check(pooled >= kPooledLiftLo && pooled <= kPooledLiftHi,
          "POOLED lift " + std::to_string(pooled) +
              " is outside the band [" + std::to_string(kPooledLiftLo) + ", " +
              std::to_string(kPooledLiftHi) +
              "]. This is the high-power form of the label question: probe "
              "selection has become correlated with fraud.");
  }

  // ------------------------- C: high degree does not mean compromise
  for (std::size_t i = 0; i < all.size(); ++i) {
    const auto &m = all[i];
    check(m.cleanShareOnProbeDevices >= kMinCleanShare,
          std::string{legs[i].name} +
              ": probe endpoints reach mostly FRAUD cards (clean share " +
              std::to_string(m.cleanShareOnProbeDevices) +
              "), so a high-degree device is still a compromise tell");
  }

  if (failures == 0) {
    std::printf("CARD-TESTING HOLDS: fan-out without a label.\n");
    return 0;
  }
  std::fprintf(stderr, "\n%d enumeration check(s) failed\n", failures);
  return EXIT_FAILURE;
}
