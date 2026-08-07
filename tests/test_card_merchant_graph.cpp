//
// tests/test_card_merchant_graph.cpp
//
// THE MERCHANT DEGREE GATE (merchant-selection-2026-08).
//
// ===================================================================
// WHY THIS FILE EXISTS, AND IT IS THE GHOST-ENDPOINT LESSON VERBATIM
//
// CLAUDE.md's standing lesson from `attacker-infra-2026-07` is
// **"A COUNT OF ENDPOINTS IS NOT A MEASUREMENT OF THE GRAPH"**: four gates,
// an acceptance script and two normative documents all asserted things about
// attacker endpoints, every one checked that endpoints were PRESENT, and not
// one measured DEGREE — so the single most valuable card-fraud graph signal
// was absent behind four greens.
//
// **The identical failure was live on the MERCHANT vertex the whole time.**
// Before this file, `grep -rE "cardsPerMerchant|merchantsPerCard|degree"
// tests/` returned nothing. What existed instead:
//
//   * `test_card_merchant_overlap` measures FRAUD-ONLY merchant share and its
//     own header says "this file bounds no measured value".
//   * `test_merchant_churn`'s `traversalRatio = touched.size() / liveStart`
//     is set CARDINALITY, which is invariant under any reweighting of row
//     mass across the same key set. A corpus with one merchant on 51% of
//     cards scores identically to a perfectly flat one.
//   * `Summary::merchantCount` has exactly one consumer in the repository: a
//     `std::printf` in `src/app/main.cpp`. It is never asserted.
//   * `test_card_endpoint_graph` DOES measure degree — for devices and IPs.
//     Its `summarize<Key>` template compiles unchanged against merchants and
//     was never pointed at them.
//
// The defect that reached the owner: at 8,000 people over 20 years one
// merchant sat on **51%** of all 68,618 cards, and the monthly evolver took
// that to 85%. Every gate stayed green.
//
// ===================================================================
// WHAT IS BOUNDED AND WHAT IS ONLY PRINTED, AND WHY THAT SPLIT IS REAL
//
// Mean reach is an identity, not a parameter:
//
//     mean cards-per-merchant / |cards|  ==  distinct merchants-per-card
//                                            / |live merchants|
//
// So a configuration whose per-cardholder breadth approaches its live
// merchant count has mean reach approaching 1, and **no top-1 ceiling is
// achievable in it at any parameter setting.** At 8,000 people over 20 years
// the live catalogue ends at ~439 merchants while a realistic 20-year
// cardholder touches ~460 distinct merchants. That configuration is
// arithmetically incoherent with realistic breadth and cannot be gated into
// coherence.
//
// The consequence for THIS file: the heavy cards-per-merchant TAIL only
// materialises once the live catalogue is large relative to the set size
// (measured p99/mean 2.7 at pop 8,000 versus 14.2 at pop 500,000). Gate legs
// cannot run at 500,000 people, so sub-gate C **PRINTS** the tail statistics
// at both legs and bounds only the direction. Printing a number the gate
// cannot bound is the honest form; asserting a band the leg cannot reach is
// how `test_card_baselines` came to score 0.0000 on merchant-shortcut recall
// — a pass earned by having no data.
//

#include "phantomledger/activity/spending/market/commerce/affinity.hpp"
#include "phantomledger/activity/spending/market/commerce/local_pools.hpp"
#include "phantomledger/activity/spending/market/commerce/reach.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/geo/residence.hpp"
#include "phantomledger/synth/merchants/make.hpp"
#include "phantomledger/synth/merchants/place.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include "window_leg_support.hpp"

#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;
namespace commerce = pl::activity::spending::market::commerce;

using pltest::LegOptions;
using Txn = pltest::Txn;

namespace {

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// The card rail, matching `test_card_merchant_overlap`'s definition so the two
// gates describe the same population.
[[nodiscard]] bool isCardRail(const Txn &t) {
  return t.session.channel.value ==
         channels::tag(channels::Legit::cardPurchase).value;
}

struct Shape {
  std::size_t merchants = 0;
  std::size_t cards = 0;
  double top1Penetration = 0.0;
  std::size_t above25 = 0;
  std::size_t above50 = 0;
  double medianOverMean = 0.0;
  double p99OverMean = 0.0;
  double fano = 0.0;
  double meanWithinCardTop1 = 0.0;
  // The 1/distinctMerchants baseline over the same cards. Sub-gate D is a
  // RATIO against this rather than an absolute band, because the absolute
  // value moves with whatever set size the evolver settles at while the ratio
  // does not.
  //
  // NOTE, and it cost a wrong band once: a UNIFORM pick does NOT score 1.0 on
  // this ratio. `mean(1/F)` is not `1/mean(F)` (Jensen), and more importantly
  // the MAX of a multinomial over F cells with a finite row count sits well
  // above 1/F by fluctuation alone. MEASURED at these legs: a uniform pick
  // scores 2.118 / 2.180, not 1.0. The band is set from that measurement.
  double meanUniformBaseline = 0.0;
  double concentrationRatio = 0.0;
  double pairShareProbability = 0.0;
};

[[nodiscard]] Shape measureShape(const std::vector<Txn> &rows) {
  // merchant -> distinct funding accounts, and account -> per-merchant counts.
  std::map<pl::entity::Key, std::set<pl::entity::Key>> cardsPerMerchant;
  std::map<pl::entity::Key, std::map<pl::entity::Key, std::size_t>> visits;
  std::set<pl::entity::Key> cards;

  for (const auto &t : rows) {
    if (!isCardRail(t)) {
      continue;
    }
    cardsPerMerchant[t.target].insert(t.source);
    ++visits[t.source][t.target];
    cards.insert(t.source);
  }

  Shape out;
  out.merchants = cardsPerMerchant.size();
  out.cards = cards.size();
  if (out.merchants == 0 || out.cards == 0) {
    return out;
  }

  const double cardCount = static_cast<double>(out.cards);

  std::vector<double> degrees;
  degrees.reserve(out.merchants);
  double sumSquaredShare = 0.0;
  for (const auto &[merchant, holders] : cardsPerMerchant) {
    (void)merchant;
    const double share = static_cast<double>(holders.size()) / cardCount;
    degrees.push_back(static_cast<double>(holders.size()));
    out.top1Penetration = std::max(out.top1Penetration, share);
    if (share > 0.25) {
      ++out.above25;
    }
    if (share > 0.50) {
      ++out.above50;
    }
    sumSquaredShare += share * share;
  }

  // P(two random cards share at least one merchant), Poisson approximation on
  // the expected number of shared merchants. 1.000 is the pre-round value at
  // pop 8,000 over 20 years and it is what makes "shared merchant" carry zero
  // bits for a graph model.
  out.pairShareProbability = 1.0 - std::exp(-sumSquaredShare);

  std::sort(degrees.begin(), degrees.end());
  double total = 0.0;
  for (const double d : degrees) {
    total += d;
  }
  const double mean = total / static_cast<double>(degrees.size());
  const double median = degrees[degrees.size() / 2];
  const double p99 = degrees[static_cast<std::size_t>(
      0.99 * static_cast<double>(degrees.size() - 1))];
  double variance = 0.0;
  for (const double d : degrees) {
    variance += (d - mean) * (d - mean);
  }
  variance /= static_cast<double>(degrees.size());

  out.medianOverMean = mean > 0.0 ? median / mean : 0.0;
  out.p99OverMean = mean > 0.0 ? p99 / mean : 0.0;
  out.fano = mean > 0.0 ? variance / mean : 0.0;

  // Within-card top-1 visit share: the quantity Krumme et al. measure at 13%
  // (North American issuer) and 22% (European). A UNIFORM pick inside the
  // favourite row — what this round replaced — scores 1/F, i.e. 5.3% at the
  // seeded mean set size of 19 and 2.6% at the pre-round saturated 38.
  double shareTotal = 0.0;
  double uniformTotal = 0.0;
  std::size_t counted = 0;
  for (const auto &[card, byMerchant] : visits) {
    (void)card;
    std::size_t cardTotal = 0;
    std::size_t top = 0;
    for (const auto &[merchant, n] : byMerchant) {
      (void)merchant;
      cardTotal += n;
      top = std::max(top, n);
    }
    // Cards with a handful of rows carry no distributional information and
    // would bias the mean upward (a 1-row card scores 1.0 by construction).
    // The same guard excludes cards with a single distinct merchant, where the
    // uniform baseline is also 1.0 and the ratio is undefined.
    if (cardTotal < 20 || byMerchant.size() < 2) {
      continue;
    }
    shareTotal += static_cast<double>(top) / static_cast<double>(cardTotal);
    uniformTotal += 1.0 / static_cast<double>(byMerchant.size());
    ++counted;
  }
  if (counted > 0) {
    const double denom = static_cast<double>(counted);
    out.meanWithinCardTop1 = shareTotal / denom;
    out.meanUniformBaseline = uniformTotal / denom;
    out.concentrationRatio =
        out.meanUniformBaseline > 0.0
            ? out.meanWithinCardTop1 / out.meanUniformBaseline
            : 0.0;
  }

  return out;
}

void report(const char *label, const Shape &s) {
  std::printf("\n=== %s ===\n", label);
  std::printf("  card-rail merchants %zu, cards %zu\n", s.merchants, s.cards);
  std::printf("  TOP-1 CARD PENETRATION      %.4f   (ceiling %.2f)\n",
              s.top1Penetration, commerce::kMaxTop1Reach);
  std::printf("  hubs >25%% / >50%%            %zu / %zu\n", s.above25,
              s.above50);
  std::printf(
      "  WITHIN-CARD TOP-1 SHARE     %.4f   (Krumme 0.13 NA / 0.22 EU)\n",
      s.meanWithinCardTop1);
  std::printf("  vs UNIFORM baseline         %.4f   ratio %.3f  (uniform "
              "measures 1.79-1.83)\n",
              s.meanUniformBaseline, s.concentrationRatio);
  std::printf("  P(2 cards share a merchant) %.4f\n", s.pairShareProbability);
  std::printf("  cards/merchant  median/mean %.4f   p99/mean %.2f   Fano %.2f"
              "   [PRINTED, not bounded — see header]\n",
              s.medianOverMean, s.p99OverMean, s.fano);
}

} // namespace

namespace {

// PRODUCTION-SCALE SUB-GATES, WITHOUT A PRODUCTION-SCALE CORPUS.
//
// The corpus legs below cannot run at the owner's 500,000-person target, and
// the quantities this round is about only become coherent there — a physical
// outlet's reach ceiling is bounded by its own AREA's share of the population,
// so it depends on merchants-per-area, which is ~3 at pop 300 and ~317 at pop
// 500,000. Gating only on small legs would band a regime production never uses.
//
// So these two sub-gates drive the REAL sampler (`makeCatalog` +
// `placeGeography` + `calibrateReachModel` + `MembershipSampler`) against the
// REAL residence model at both populations directly. No ledger, no fold,
// seconds to run — the mechanism is exactly the production one, only the corpus
// is absent.
struct ScaleShape {
  double top1 = 0.0;
  std::size_t above25 = 0;
  std::size_t above50 = 0;
  double meanMiles = 0.0;
  double within50 = 0.0;
  double onlineShare = 0.0;
  std::size_t topPhysicalAreas = 0;
  double worstDiscarded = 0.0;
  double poolMiB = 0.0;
};

[[nodiscard]] ScaleShape measureAtScale(int population, int people, int year) {
  namespace ge = pl::entity::geography;
  const auto &geo = pl::synth::geo::geography();
  const pl::synth::geo::ResidenceSampler residence{geo};

  auto rng = pl::random::Rng::fromSeed(0xDEADBEEFULL);
  auto catalog = pl::synth::merchants::makeCatalog(rng, population);
  pl::synth::merchants::placeGeography(catalog, 0xC0FFEEULL);

  std::vector<double> volume;
  volume.reserve(catalog.records.size());
  for (const auto &r : catalog.records) {
    volume.push_back(r.weight);
  }
  const auto model = commerce::calibrateReachModel(volume, 30.0);

  std::vector<ge::GeoAreaId> homes;
  for (std::size_t a = 1; a <= geo.size(); ++a) {
    homes.push_back(static_cast<ge::GeoAreaId>(a));
  }
  const auto sampler =
      commerce::MembershipSampler::build(catalog, geo, homes, model.membership,
                                         30.0, commerce::cnpShareForYear(year));

  auto prng = pl::random::Rng::fromSeed(4242ULL);
  std::map<std::uint32_t, std::size_t> holders;
  std::map<std::uint32_t, std::set<ge::GeoAreaId>> holderAreas;
  double milesSum = 0.0;
  std::size_t physical = 0;
  std::size_t within50 = 0;

  for (int p = 0; p < people; ++p) {
    const auto home = residence.sample(prng, pl::locale::Country::us);
    std::set<std::uint32_t> row;
    for (int t = 0; t < 250 && row.size() < 19; ++t) {
      row.insert(sampler.sample(home, prng.nextDouble()));
    }
    for (const auto idx : row) {
      ++holders[idx];
      holderAreas[idx].insert(home);
      const auto loc = catalog.records[idx].location;
      if (!ge::validArea(loc) || !geo.contains(loc)) {
        continue;
      }
      ++physical;
      const double d = ge::distanceMiles(geo.at(home), geo.at(loc));
      milesSum += d;
      if (d <= 50.0) {
        ++within50;
      }
    }
  }

  ScaleShape out;
  out.onlineShare = sampler.onlineShare();
  out.worstDiscarded = sampler.physical().worstDiscardedMass();
  out.poolMiB = static_cast<double>(sampler.memoryBytes()) / 1048576.0;
  double topPhysical = 0.0;
  std::uint32_t topPhysicalIdx = 0;
  for (const auto &[idx, count] : holders) {
    const double share =
        static_cast<double>(count) / static_cast<double>(people);
    out.top1 = std::max(out.top1, share);
    if (share > 0.25)
      ++out.above25;
    if (share > 0.50)
      ++out.above50;
    const auto loc = catalog.records[idx].location;
    const bool online = catalog.records[idx].footprint ==
                            pl::entity::merchant::Footprint::online ||
                        !ge::validArea(loc) || !geo.contains(loc);
    if (!online && share > topPhysical) {
      topPhysical = share;
      topPhysicalIdx = idx;
    }
  }
  out.topPhysicalAreas = holderAreas.count(topPhysicalIdx)
                             ? holderAreas[topPhysicalIdx].size()
                             : 0;
  out.meanMiles = physical ? milesSum / static_cast<double>(physical) : 0.0;
  out.within50 =
      physical ? static_cast<double>(within50) / static_cast<double>(physical)
               : 0.0;
  return out;
}

} // namespace

int main() {
  // ------------------------------------------------------------------
  // SUB-GATE F — THE CITED ARITHMETIC, EVALUATED RATHER THAN RESTATED.
  //
  // `bls-citation-2026-07` rule 2: A DERIVATION IN A COMMENT IS NOT A
  // DERIVATION UNTIL SOMETHING EVALUATES IT. `commerce/affinity.hpp` claims
  // that a deterministic Zipf rank law at alpha = 0.80 predicts a 13.4% top-1
  // visit share at Krumme et al.'s own reported median set size of 64
  // merchants, reproducing their separately-reported 13% without being fitted
  // to it. That claim is checked here, against the shipped constant.
  {
    double sum = 0.0;
    for (int r = 1; r <= 64; ++r) {
      sum += std::pow(static_cast<double>(r), -commerce::kVisitZipfAlpha);
    }
    const double predicted = 1.0 / sum;
    std::printf("sub-gate F: sum_{r=1..64} r^-%.2f = %.4f -> top-1 = %.4f "
                "(Krumme published 0.13)\n",
                commerce::kVisitZipfAlpha, sum, predicted);
    check(std::abs(predicted - 0.13) < 0.02,
          "sub-gate F: the shipped Zipf exponent must reproduce Krumme's "
          "published 13% top-1 visit share at their reported median set size "
          "of 64; got " +
              std::to_string(predicted));
  }

  // ------------------------------------------------------------------
  // SUB-GATE E — THE REACH SOLVE MUST NOT SILENTLY SATURATE.
  //
  // CLAUDE.md rule 6: SIZING CONSTANTS THAT CONVERT A DISTRIBUTION INTO A
  // COUNT MUST BE MEASURED, NOT DERIVED — the attacker concurrency floor was
  // under-delivered twice and both times the disposition was to fix the
  // construction, never to widen the band. `calibrateReachModel` solves an
  // exponent by bisection, and a solved constant that pins at a bound while
  // reporting success is exactly that failure. Both bounds are exercised.
  {
    // A heavy-tailed law, where flattening is genuinely needed.
    std::vector<double> heavy(570);
    for (std::size_t i = 0; i < heavy.size(); ++i) {
      heavy[i] = 1.0 / std::pow(static_cast<double>(i + 1), 1.6);
    }
    const auto model = commerce::calibrateReachModel(heavy, 30.0);
    std::printf("sub-gate E: heavy law -> gamma %.4f, realized top-1 %.4f "
                "(target %.2f)\n",
                model.gamma, model.realizedTop1, commerce::kTargetTop1Reach);
    check(model.gamma > 0.0 && model.gamma < 1.0,
          "sub-gate E: a heavy volume law must need a STRICTLY INTERIOR "
          "flattening exponent; gamma at a bound means the solve saturated");
    check(std::abs(model.realizedTop1 - commerce::kTargetTop1Reach) < 0.005,
          "sub-gate E: the solve must hit the declared reach target");

    // A law already flatter than the target: gamma must pin at 1.0 (no
    // flattening) and realized reach must come in UNDER target rather than
    // being pushed up to it. Over-correcting toward a hub would be the
    // dangerous direction.
    std::vector<double> flat(26000, 1.0);
    const auto flatModel = commerce::calibrateReachModel(flat, 30.0);
    check(flatModel.gamma == 1.0,
          "sub-gate E: a law already flatter than target must not be "
          "flattened further");
    check(flatModel.realizedTop1 <= commerce::kTargetTop1Reach,
          "sub-gate E: reach must never be pushed UP to the target");

    // The achievability precondition, both signs. This is the identity in the
    // header: a small catalogue against a large set size cannot host any
    // top-1 ceiling.
    check(commerce::reach::reachAchievable(26000, 30.0,
                                           commerce::kTargetTop1Reach),
          "sub-gate E: 26,000 live merchants at set size 30 must admit a "
          "0.12 reach target");
    check(!commerce::reach::reachAchievable(200, 30.0,
                                            commerce::kTargetTop1Reach),
          "sub-gate E: 200 live merchants at set size 30 must be reported "
          "UNACHIEVABLE rather than silently accepted — mean reach alone is "
          "already ~0.14 there");
  }

  // ------------------------------------------------------------------
  // SUB-GATE I — THE DATED CARD-NOT-PRESENT SHARE.
  //
  // A flat share is the `burst-rate-2026-07` failure in another costume: card
  // e-commerce barely existed in 1991, so one constant cannot be right at both
  // ends of a 20-year window. This pins the CITED anchor and the SHAPE.
  //
  // DISARM: replace `cnpShareForYear` with a constant. Reds on monotonicity
  // (1991 would equal 2022) and on the era-contrast floor.
  {
    const double y1991 = commerce::cnpShareForYear(1991);
    const double y2005 = commerce::cnpShareForYear(2005);
    const double y2019 = commerce::cnpShareForYear(2019);
    const double y2022 = commerce::cnpShareForYear(2022);
    std::printf("\nsub-gate I: CNP share 1991 %.4f  2005 %.4f  2019 %.4f  "
                "2022 %.4f\n",
                y1991, y2005, y2019, y2022);

    // The one CITED level: Fed Payments Study 2022 puts in-person at 63.8% of
    // GP card payments BY NUMBER, so remote is 36.2%.
    check(std::abs(y2022 - 0.362) < 0.001,
          "sub-gate I: 2022 must equal the cited Fed CNP share 0.362, got " +
              std::to_string(y2022));
    // Monotone non-decreasing, and the era contrast must be real — a decade of
    // e-commerce growth is not a rounding difference.
    check(y1991 < y2005 && y2005 < y2019 && y2019 < y2022,
          "sub-gate I: the CNP share must rise monotonically across eras");
    check(y2022 / y1991 > 10.0,
          "sub-gate I: 2022 must carry at least 10x the 1991 CNP share; a flat "
          "constant cannot be right at both ends of a multi-decade window");
    // And in-person must remain the MAJORITY at every modelled year — the
    // intuition that online overtook in-person is not what the count data says.
    check(y2022 < 0.50,
          "sub-gate I: card-not-present must stay below half of card payments "
          "by NUMBER; the Fed measures in-person at 63.8% in 2022");
  }

  // ------------------------------------------------------------------
  // SUB-GATES G and H — PRODUCTION SCALE, and TWO populations because
  // `burst-rate` rule 1 applies: reach concentration here is EMERGENT from
  // merchants-per-area, so a single population cannot distinguish a correct
  // mechanism from a small-world artifact.
  {
    struct Scale {
      const char *name;
      int population;
      int people;
      bool bound;
    };
    // pop 8,000 is PRINTED, not bounded: ~570 records over 71 areas is ~7
    // physical merchants per area against a 19-merchant favourite set, so the
    // local pool cannot satisfy the set and reach concentrates for arithmetic
    // reasons no parameter can fix. That is registered, not engineered around.
    const Scale scales[] = {
        {"pop 8,000 (PRINTED, incoherent — see below)", 8000, 8000, false},
        {"pop 500,000 (BOUNDED — the owner's target)", 500000, 20000, true},
    };
    for (const auto &sc : scales) {
      const auto shape = measureAtScale(sc.population, sc.people, 2022);
      std::printf("\nsub-gate G/H: %s\n", sc.name);
      std::printf("    top-1 reach %.4f   hubs >25%%/>50%%: %zu/%zu   "
                  "online share %.4f\n",
                  shape.top1, shape.above25, shape.above50, shape.onlineShare);
      std::printf("    home->merchant  mean %.1f mi   P(<=50mi) %.4f   "
                  "top physical spans %zu areas\n",
                  shape.meanMiles, shape.within50, shape.topPhysicalAreas);
      std::printf("    pools %.3f MiB   cutoff discarded %.2e\n", shape.poolMiB,
                  shape.worstDiscarded);

      // H — LOCALITY. Bounded at BOTH scales, because it is the defect the
      // owner identified and it is scale-robust: a physical favourite must be
      // near home whatever the catalogue size. Pre-round this measured a mean
      // of 1,206 MILES with 4.96% inside 50 miles, and the fraud card-present
      // rail is distance-decayed to 0-11 miles — a ~7x label shortcut from one
      // exported feature, INVERTED versus real card-present fraud.
      // DISARM: build the sampler over a national CDF instead of the local
      // pools. Reds immediately at both scales.
      check(shape.within50 >= 0.80,
            std::string(sc.name) + ": only " + std::to_string(shape.within50) +
                " of physical favourites are within 50 miles of home; "
                "membership must be geography-conditioned or "
                "distance-from-home becomes a label shortcut");
      check(shape.meanMiles <= 100.0,
            std::string(sc.name) + ": mean home-to-favourite distance " +
                std::to_string(shape.meanMiles) + " mi exceeds 100");

      // The cutoff must not be quietly throwing away reachable merchants.
      check(shape.worstDiscarded < 1e-6,
            std::string(sc.name) + ": the inter-area cutoff discards " +
                std::to_string(shape.worstDiscarded) +
                " of a home's reachable mass; size kMaxNearbyAreas by "
                "measurement, not by guess");

      if (!sc.bound) {
        continue;
      }
      // G — REACH, bounded only where merchants-per-area can host the set.
      check(shape.top1 <= commerce::kMaxTop1Reach,
            std::string(sc.name) + ": top-1 reach " +
                std::to_string(shape.top1) + " exceeds the ceiling");
      check(shape.above50 == 0,
            std::string(sc.name) + ": no merchant may reach half the cards");
      check(shape.above25 <= 3, std::string(sc.name) +
                                    ": at most 3 merchants above 25% reach "
                                    "(found " +
                                    std::to_string(shape.above25) + ")");
    }
  }

  const auto pools = pltest::buildPoolSet(1234567);

  // TWO LEGS, and the reason is `burst-rate-2026-07` rule 1: a scale-invariant
  // concentration and a small-world artifact are indistinguishable at one
  // population. Leg A is the existing gate convention (and the regime where
  // the tail is arithmetically flat); leg B is the largest population a gate
  // can afford and is where the tail begins to appear.
  struct Leg {
    const char *name;
    std::int32_t population;
    std::int64_t days;
  };
  const Leg legs[] = {
      {"leg-A: pop 300, 730d", 300, 730},
      {"leg-B: pop 2000, 730d", 2000, 730},
  };

  for (const auto &leg : legs) {
    LegOptions opt;
    opt.seed = 1234567;
    opt.window.start = pl::time::makeTime(pl::time::CalendarDate{1991, 1, 1});
    opt.window.days = leg.days;
    opt.population = leg.population;
    opt.withBaseRoutines = true;
    opt.withFamily = true;

    pltest::announceLeg(leg.name);
    const auto result = pltest::runLeg(pools, opt);

    // WORLD SHAPE FIRST, per the join-cohort round: a zero here means the leg
    // rebuilt a population production never generates and every number below
    // describes a world that does not ship.
    check(result.joiners > 0, std::string(leg.name) +
                                  ": the leg must carry the production join "
                                  "cohort (joiners " +
                                  std::to_string(result.joiners) + ")");

    const auto shape = measureShape(result.rows);
    report(leg.name, shape);

    // Well-definedness before any band means anything.
    check(shape.merchants > 0 && shape.cards > 0,
          std::string(leg.name) + ": the card rail must resolve to merchants "
                                  "and cards");

    // --------------------------------------------------------------
    // SUB-GATE A — TOP-1 CARD PENETRATION, PRINTED AT THESE LEGS.
    //
    // NOT BOUNDED HERE, and the reason is arithmetic rather than convenience.
    // Once membership is geography-conditioned, a physical outlet's reach is
    // bounded by its own AREA's share of the population — so the achievable
    // ceiling depends on MERCHANTS PER AREA, which is ~3 at pop 300 and ~7 at
    // pop 8,000 against a 19-merchant favourite set. Below roughly pop 100,000
    // the local pool cannot satisfy the set, everyone in a region draws from
    // the same handful of outlets, and reach concentrates for reasons no
    // parameter can fix. Measured: 0.359 (pop 300), 0.293 (pop 2,000), 0.234
    // (pop 8,000), 0.057 (pop 500,000).
    //
    // The ceiling IS bounded, at the population that matters, by sub-gate G
    // above — which drives the same sampler over the same catalogue without
    // needing a corpus. Banding it here instead would have pinned a regime
    // production never runs, which is the `test_card_baselines` mistake.
    std::printf("  (top-1 printed, not bounded at this leg — sub-gate G bounds "
                "it at pop 500,000)\n");

    // What IS bounded at every scale: no merchant may reach half the cards.
    // That survives the small-population incoherence and is the hard failure.
    check(shape.above50 == 0, std::string(leg.name) +
                                  ": no merchant may sit on more than half "
                                  "the cards (found " +
                                  std::to_string(shape.above50) + ")");

    // --------------------------------------------------------------
    // SUB-GATE D — WITHIN-CARD VISIT CONCENTRATION, AS A RATIO AGAINST THE
    // UNIFORM BASELINE MEASURED ON THE SAME CARDS.
    //
    // THE FIRST VERSION OF THIS CHECK WAS AN ABSOLUTE BAND AND THE DISARM
    // PASSED IT. That is the finding worth recording. Banding the raw share at
    // [0.06, 0.35] around Krumme's cited 0.13-0.22 looked reasonable, but
    // reverting the pick to `rng_.choiceIndex` scored 0.1024 — a real 1.8x
    // drop from the armed 0.1825, and still comfortably inside the band. The
    // absolute value moves with whatever set size the evolver settles at
    // (these legs realize ~20 distinct merchants per card, not the seeded
    // range's mean of 19 after churn), so an absolute band cannot separate the
    // mechanism from the set size.
    //
    // The ratio can, because the baseline is recomputed from the SAME cards.
    // BOTH ENDS ARE MEASURED, not derived — and the derivation would have been
    // wrong: a uniform pick does not score 1.0 here, because the max of a
    // multinomial with a finite row count exceeds 1/F by fluctuation alone.
    //
    //   armed     2.633 (leg-A) / 2.740 (leg-B)
    //   disarmed  1.791 (leg-A) / 1.828 (leg-B)
    //
    // Floor at 2.30: 13% below the armed value, 26% above the disarm. Sizing a
    // band from the analytic 1.0 would have shipped a check that cannot fail —
    // the same trap `card-churn-2026-07` rule 3 records for the binomial.
    //
    // RE-MEASURED ONCE, and that is worth recording as process rather than
    // hidden. The floor was 2.90 against an armed 3.65 when membership was
    // geography-FREE. Localising membership and raising the card-not-present
    // share to the Fed's dated anchor changed the row composition, so both ends
    // moved down together and the old floor would have failed a correct build.
    // A band measured against a superseded construction is not a measurement.
    //
    // The absolute share is still asserted, but only against the loose
    // plausibility envelope — it is the ratio that carries the claim.
    //
    // DISARM: restore `rng_.choiceIndex(favRow.size())` in
    // `routing/payments.cpp`. Scores 2.118 / 2.180 and reds this at both legs.
    if (shape.concentrationRatio > 0.0) {
      check(shape.concentrationRatio >= 2.30,
            std::string(leg.name) + ": within-card visit concentration ratio " +
                std::to_string(shape.concentrationRatio) +
                " is below 2.30 — a cardholder's merchant visits are a Zipf "
                "rank law (Krumme et al. 2013, alpha 0.80), and a uniform pick "
                "measures 1.79-1.83 here");
      check(shape.meanWithinCardTop1 <= 0.35,
            std::string(leg.name) + ": within-card top-1 visit share " +
                std::to_string(shape.meanWithinCardTop1) +
                " exceeds 0.35, above even the European issuer's 0.22");
    }
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "\n%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("\ntest_card_merchant_graph: merchant degree bounded\n");
  return 0;
}
