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

#include "phantomledger/activity/spending/market/bootstrap.hpp"
#include "phantomledger/activity/spending/market/commerce/affinity.hpp"
#include "phantomledger/activity/spending/market/commerce/local_pools.hpp"
#include "phantomledger/activity/spending/market/commerce/reach.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/geo/residence.hpp"
#include "phantomledger/synth/merchants/make.hpp"
#include "phantomledger/synth/merchants/outlets.hpp"
#include "phantomledger/synth/merchants/place.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/merchants/names.hpp"

#include "window_leg_support.hpp"

#include <algorithm>
#include <array>
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
  std::size_t topPhysicalBrandAreas = 0;
  double worstDiscarded = 0.0;
  double poolMiB = 0.0;
};

constexpr std::uint64_t kCatalogSeed = 0xDEADBEEFULL;
constexpr std::uint64_t kGeoSeed = 0xC0FFEEULL;

// The catalogue exactly as `buildMerchants` builds it before churn: a gate
// that rebuilt it without the outlet expansion would measure a construction
// production no longer ships (merchant-selection-2026-08: a band measured
// against a superseded construction is not a measurement).
[[nodiscard]] pl::entity::merchant::Catalog productionCatalog(int population,
                                                              bool outlets) {
  auto rng = pl::random::Rng::fromSeed(kCatalogSeed);
  auto catalog = pl::synth::merchants::makeCatalog(rng, population);
  pl::synth::merchants::placeGeography(catalog, kGeoSeed);
  if (outlets) {
    pl::synth::merchants::expandOutlets(
        catalog, pl::synth::merchants::coreCountFor(population, {}), kGeoSeed);
  }
  return catalog;
}

[[nodiscard]] ScaleShape measureAtScale(int population, int people, int year) {
  namespace ge = pl::entity::geography;
  const auto &geo = pl::synth::geo::geography();
  const pl::synth::geo::ResidenceSampler residence{geo};

  const auto catalog = productionCatalog(population, true);

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
  // The same outlet's BRAND: every home area holding any outlet of it.
  {
    const auto brand = catalog.records[topPhysicalIdx].brandOf();
    std::set<ge::GeoAreaId> brandAreas;
    for (const auto &[idx, areas] : holderAreas) {
      if (catalog.records[idx].brandOf() == brand) {
        brandAreas.insert(areas.begin(), areas.end());
      }
    }
    out.topPhysicalBrandAreas = brandAreas.size();
  }
  out.meanMiles = physical ? milesSum / static_cast<double>(physical) : 0.0;
  out.within50 =
      physical ? static_cast<double>(within50) / static_cast<double>(physical)
               : 0.0;
  return out;
}

// ===================================================================
// SUB-GATE J: CHAIN OUTLETS (outlets-frequency-2026-09), production scale.
//
// `expandOutlets` turns each physical chain brand into outlet records. What
// this bounds, and why each part is here:
//   J1  the shared entity stream and every base record's placement lanes are
//       untouched (`makeCatalog`'s draw count is load-bearing,
//       merchant-churn-2026-07);
//   J2  a brand's outlet weights sum to its pre-expansion weight, so every
//       volume law reads the same totals;
//   J3  domain predicates on every outlet and every brand;
//   J4  the UNFITTED check: the rule has no free constant, and its chain
//       shares land on SUSB 2022 Retail Trade, where firms with 500+
//       employees hold 0.632 of receipts and 0.313 of establishments;
//   J5  the disarm (no expansion, the pre-round catalogue) scores 0 on both
//       shares, so J4 cannot pass on a catalogue without outlets;
//   J6  printed only: the largest chain and the small-population legs, which
//       are not the production regime (the core floor binds there, so their
//       shares cannot be bounded; merchant-selection-2026-08).
// ===================================================================
struct ChainShape {
  std::size_t records = 0;
  std::size_t chains = 0;
  std::size_t added = 0;
  double volumeShare = 0.0;
  double establishmentShare = 0.0;
  double recordsPer10k = 0.0;
  std::size_t maxOutlets = 0;
  std::size_t maxAreas = 0;
  std::size_t maxPerArea = 0;
};

// A card-present retail establishment: the SUSB comparison population.
[[nodiscard]] bool physicalRetail(const pl::entity::merchant::Record &r) {
  using F = pl::entity::merchant::Footprint;
  return pl::synth::merchants::isOutletCategory(r.category) &&
         (r.footprint == F::localOutlet || r.footprint == F::regionalOutlet) &&
         pl::entity::geography::validArea(r.location);
}

[[nodiscard]] ChainShape chainShape(const pl::entity::merchant::Catalog &c,
                                    int population) {
  ChainShape out;
  out.records = c.records.size();
  out.recordsPer10k = static_cast<double>(out.records) * 10000.0 /
                      static_cast<double>(population);
  double volAll = 0.0;
  double volChain = 0.0;
  std::size_t estAll = 0;
  std::size_t estChain = 0;
  std::map<std::uint64_t, std::map<pl::entity::geography::GeoAreaId,
                                   std::size_t>>
      byBrand;
  for (const auto &r : c.records) {
    if (r.brand.value != 0) {
      ++byBrand[r.brand.value][r.location];
    }
    if (!physicalRetail(r)) {
      continue;
    }
    volAll += r.weight;
    ++estAll;
    if (r.brand.value != 0) {
      volChain += r.weight;
      ++estChain;
    }
  }
  out.chains = byBrand.size();
  for (const auto &[brand, areas] : byBrand) {
    (void)brand;
    std::size_t n = 0;
    for (const auto &[area, count] : areas) {
      (void)area;
      n += count;
      out.maxPerArea = std::max(out.maxPerArea, count);
    }
    out.added += n - 1;
    out.maxOutlets = std::max(out.maxOutlets, n);
    out.maxAreas = std::max(out.maxAreas, areas.size());
  }
  out.volumeShare = volAll > 0.0 ? volChain / volAll : 0.0;
  out.establishmentShare =
      estAll > 0 ? static_cast<double>(estChain) / static_cast<double>(estAll)
                 : 0.0;
  return out;
}

// SUSB 2022 Retail Trade (NAICS 44-45), firms with 500+ employees:
// receipts 0.632 and establishments 0.313 (645,404 firms, 1,045,890
// establishments, $6,850.9B receipts; <500 firms 718,945 establishments and
// $2,523.6B). Bands +-0.10 around each: the rule is unfitted, so the bands
// say "the right regime", not "a calibrated value".
constexpr double kSusbChainReceipts = 0.632;
constexpr double kSusbChainEstablishments = 0.313;
constexpr double kChainShareBand = 0.10;
// CBP 2022 employer establishments 248.4 per 10k and Nilson YE2024
// card-accepting locations 999.7 per 10k (merchant-selection-2026-08).
constexpr double kMinRecordsPer10k = 248.0;
constexpr double kMaxRecordsPer10k = 1000.0;

[[nodiscard]] bool chainSharesInBand(const ChainShape &s) {
  return std::abs(s.volumeShare - kSusbChainReceipts) <= kChainShareBand &&
         std::abs(s.establishmentShare - kSusbChainEstablishments) <=
             kChainShareBand &&
         s.recordsPer10k >= kMinRecordsPer10k &&
         s.recordsPer10k <= kMaxRecordsPer10k;
}

void subGateJ() {
  namespace sm = pl::synth::merchants;
  namespace ge = pl::entity::geography;
  using F = pl::entity::merchant::Footprint;
  const auto &geo = pl::synth::geo::geography();
  constexpr int kPop = 500000;
  const int core = sm::coreCountFor(kPop, {});

  // J1: two streams from one seed; only one catalogue is expanded.
  auto rngA = pl::random::Rng::fromSeed(kCatalogSeed);
  auto rngB = pl::random::Rng::fromSeed(kCatalogSeed);
  auto armed = sm::makeCatalog(rngA, kPop);
  auto plain = sm::makeCatalog(rngB, kPop);
  sm::placeGeography(armed, kGeoSeed);
  sm::placeGeography(plain, kGeoSeed);
  sm::expandOutlets(armed, core, kGeoSeed);
  const auto nextA = rngA.nextU64();
  const auto nextB = rngB.nextU64();
  std::size_t baseMoved = 0;
  for (std::size_t i = 0; i < plain.records.size(); ++i) {
    const auto &a = armed.records[i];
    const auto &b = plain.records[i];
    baseMoved += (a.label != b.label || a.counterpartyId != b.counterpartyId ||
                  a.category != b.category || a.footprint != b.footprint ||
                  a.location != b.location)
                     ? 1U
                     : 0U;
  }
  std::printf("\nsub-gate J (outlets, pop %d):\n", kPop);
  std::printf("  J1 shared stream next u64 %016llx / %016llx; base records "
              "moved %zu of %zu\n",
              static_cast<unsigned long long>(nextA),
              static_cast<unsigned long long>(nextB), baseMoved,
              plain.records.size());
  check(nextA == nextB,
        "J1: expandOutlets must not touch the shared entity stream");
  check(baseMoved == 0,
        "J1: a base record's serial, key, category, footprint or area moved; "
        "outlets must be appended on their own lanes");

  // J2: conservation per brand and in total.
  std::map<std::uint64_t, double> brandMass;
  double total = 0.0;
  for (const auto &r : armed.records) {
    total += r.weight;
    if (r.brand.value != 0) {
      brandMass[r.brand.value] += r.weight;
    }
  }
  double worstRel = 0.0;
  for (const auto &[brand, mass] : brandMass) {
    const double before = plain.records[brand - 1].weight;
    worstRel = std::max(worstRel, std::abs(mass - before) / before);
  }
  std::printf("  J2 brand mass worst relative error %.2e; catalogue total "
              "%.15f\n",
              worstRel, total);
  check(worstRel <= 1e-14, "J2: a brand's outlet weights must sum to its "
                           "pre-expansion weight");
  check(std::abs(total - 1.0) <= 1e-12,
        "J2: the catalogue's volume weights must still sum to 1");

  // J3: domain predicates.
  std::set<pl::entity::Key> keys;
  std::size_t duplicateKeys = 0;
  std::size_t badOutlet = 0;
  std::size_t badBrand = 0;
  std::size_t outlets = 0;
  for (const auto &r : armed.records) {
    duplicateKeys += keys.insert(r.counterpartyId).second ? 0U : 1U;
    if (r.brand.value == 0) {
      continue;
    }
    if (r.brand.value > plain.records.size()) {
      ++badBrand;
      continue;
    }
    const auto &head = armed.records[r.brand.value - 1];
    // The brand is an ELIGIBLE CORE record heading its own chain: no online,
    // nationalService, biller-category or tail record ever has siblings.
    badBrand += (head.brand != head.label ||
                 !sm::isOutletEligible(head, core))
                    ? 1U
                    : 0U;
    if (r.label == head.label) {
      continue;
    }
    ++outlets;
    const bool domestic = ge::validArea(r.location) &&
                          geo.contains(r.location) &&
                          geo.at(r.location).country == pl::locale::Country::us;
    badOutlet += (!domestic ||
                  (r.footprint != F::localOutlet &&
                   r.footprint != F::regionalOutlet) ||
                  r.footprint != head.footprint ||
                  r.category != head.category ||
                  !sm::isOutletCategory(r.category) ||
                  r.counterpartyId.role != head.counterpartyId.role ||
                  r.counterpartyId.bank != head.counterpartyId.bank ||
                  r.label.value <= plain.records.size())
                     ? 1U
                     : 0U;
  }
  std::printf("  J3 %zu outlets: %zu off-domain, %zu bad brand references, "
              "%zu duplicate keys\n",
              outlets, badOutlet, badBrand, duplicateKeys);
  check(outlets > 0, "J3: the production catalogue must carry outlets");
  check(badOutlet == 0, "J3: every outlet must be a domestic, physical "
                        "outlet-category record on its brand's bank");
  check(badBrand == 0, "J3: every brand must be an eligible core record "
                       "heading its own chain");
  check(duplicateKeys == 0, "J3: every record must keep a unique key");

  // J4 / J5: the unfitted validation and its disarm.
  const auto armedShape = chainShape(armed, kPop);
  const auto plainShape = chainShape(plain, kPop);
  std::printf("  J4 chain volume share %.3f (SUSB 500+ receipts %.3f), "
              "establishment share %.3f (SUSB %.3f), records per 10k %.0f\n",
              armedShape.volumeShare, kSusbChainReceipts,
              armedShape.establishmentShare, kSusbChainEstablishments,
              armedShape.recordsPer10k);
  std::printf("  J5 DISARM (no expansion): volume %.3f, establishments %.3f, "
              "records per 10k %.0f\n",
              plainShape.volumeShare, plainShape.establishmentShare,
              plainShape.recordsPer10k);
  check(chainSharesInBand(armedShape),
        "J4: chain shares must sit within 0.10 of SUSB 2022 retail (0.632 "
        "receipts, 0.313 establishments) and records per 10k inside the "
        "CBP-Nilson band");
  check(!chainSharesInBand(plainShape),
        "J5: the catalogue without outlets must fail J4, or J4 is vacuous");

  // J6: printed only.
  std::printf("  J6 pop %d: %zu chains, +%zu outlets (+%.1f%% records); "
              "largest chain %zu outlets over %zu areas, at most %zu in one "
              "area\n",
              kPop, armedShape.chains, armedShape.added,
              100.0 * static_cast<double>(armedShape.added) /
                  static_cast<double>(plain.records.size()),
              armedShape.maxOutlets, armedShape.maxAreas,
              armedShape.maxPerArea);
  for (const int pop : {300, 2000, 8000}) {
    const auto small = chainShape(productionCatalog(pop, true), pop);
    std::printf("  J6 pop %d (PRINTED, coreFloor binds): %zu chains, +%zu "
                "outlets, volume %.3f, establishments %.3f, records per 10k "
                "%.0f\n",
                pop, small.chains, small.added, small.volumeShare,
                small.establishmentShare, small.recordsPer10k);
  }
}

// ===================================================================
// SUB-GATE K: CATEGORY FREQUENCY (outlets-frequency-2026-09).
//
// The favourite pick keeps each row's Zipf rank MULTISET and lets the
// category decide which favourite holds which rank. Driven over the real
// MembershipSampler at pop 500,000 (with outlets), 2005 and 2022, F = 19 and
// F = 30 (the seeded mean and the saturated set size):
//   K1  the multiset is bit-identical to the legacy law's, so within-card
//       top-1 is unchanged (Krumme's band is untouched by construction);
//   K2  the online visit share stays on the dated CNP series;
//   K3  the card-present split lands on the Diary of Consumer Payment Choice
//       2022 ratios (restaurant/grocery 5.4/5.5, gas/grocery 2.6/5.5);
//   K4  the four biller categories fall well below their legacy share;
//   K5  the disarm (every weight 1, the legacy frequency) fails K3;
//   K6  a merchant's weight is a property of the SET, not the slot;
//   K7  printed: the multiplicative alternative, a recorded negative result;
//   K8  printed: typical outlet volume a year, against the research figure;
//   K10 the fraud venue pool's `kCategoryVisitLift` is the measured
//       visit-to-favourite ratio (K9 is the corpus predicate further down).
// ===================================================================
struct FavRow {
  std::uint32_t person = 0;
  std::vector<std::uint32_t> merchants;
};

[[nodiscard]] std::vector<FavRow>
sampleRows(const commerce::MembershipSampler &sampler, int people,
           std::size_t setSize) {
  const pl::synth::geo::ResidenceSampler residence{pl::synth::geo::geography()};
  auto prng = pl::random::Rng::fromSeed(4242ULL);
  std::vector<FavRow> rows;
  rows.reserve(static_cast<std::size_t>(people));
  for (int p = 0; p < people; ++p) {
    const auto home = residence.sample(prng, pl::locale::Country::us);
    FavRow row{static_cast<std::uint32_t>(p), {}};
    for (int t = 0; t < 250 && row.merchants.size() < setSize; ++t) {
      const auto idx = sampler.sample(home, prng.nextDouble());
      if (std::find(row.merchants.begin(), row.merchants.end(), idx) ==
          row.merchants.end()) {
        row.merchants.push_back(idx);
      }
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

struct VisitMix {
  std::array<double, pl::merchants::kCategoryCount> visit{};
  std::array<double, pl::merchants::kCategoryCount> fav{};
  double online = 0.0;
  double top1 = 0.0;
  [[nodiscard]] double share(pl::merchants::Category c) const {
    return visit[static_cast<std::size_t>(c)];
  }
  [[nodiscard]] double billers() const {
    double b = 0.0;
    for (const auto c : pl::activity::spending::market::kBillerCategories) {
      b += share(c);
    }
    return b;
  }
};

// Per-slot weights of `row` under `rates`, or under the legacy law when
// `rates` is null.
double rowWeights(const pl::entity::merchant::Catalog &catalog,
                  const FavRow &row, const commerce::VisitRateTable *rates,
                  std::vector<double> &w) {
  const auto n = row.merchants.size();
  w.assign(n, 0.0);
  if (rates != nullptr) {
    return commerce::affinity::rankedWeights(row.merchants, row.person,
                                             catalog, *rates,
                                             commerce::kVisitZipfAlpha, w);
  }
  double total = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    w[i] = commerce::affinity::weightFor(row.person, row.merchants[i], n);
    total += w[i];
  }
  return total;
}

[[nodiscard]] VisitMix measureMix(const pl::entity::merchant::Catalog &catalog,
                                  const std::vector<FavRow> &rows,
                                  const commerce::VisitRateTable *rates) {
  VisitMix out;
  double favTotal = 0.0;
  std::vector<double> w;
  for (const auto &row : rows) {
    const double total = rowWeights(catalog, row, rates, w);
    if (!(total > 0.0)) {
      continue;
    }
    double top = 0.0;
    for (std::size_t i = 0; i < row.merchants.size(); ++i) {
      const auto &rec = catalog.records[row.merchants[i]];
      const auto c = static_cast<std::size_t>(rec.category);
      const double v = w[i] / total;
      out.visit[c] += v;
      out.fav[c] += 1.0;
      favTotal += 1.0;
      out.online += rec.footprint == pl::entity::merchant::Footprint::online
                        ? v
                        : 0.0;
      top = std::max(top, v);
    }
    out.top1 += top;
  }
  const double n = static_cast<double>(rows.size());
  for (auto &v : out.visit) {
    v /= n;
  }
  for (auto &f : out.fav) {
    f /= favTotal;
  }
  out.online /= n;
  out.top1 /= n;
  return out;
}

// DCPC 2022 non-cash in-person payments a month (SF Fed 2023 Findings,
// Figure 5): grocery and convenience 5.5, fast food 3.4 + sit-down 2.0, gas
// 2.6. The bands are the design's, +-0.2 around each DCPC ratio.
constexpr double kDcpcRestaurantOverGrocery = 5.4 / 5.5;
constexpr double kDcpcFuelOverGrocery = 2.6 / 5.5;

[[nodiscard]] bool dcpcSplitHolds(const VisitMix &m) {
  using C = pl::merchants::Category;
  const double g = m.share(C::grocery);
  const double groceryLift = g / m.fav[static_cast<std::size_t>(C::grocery)];
  const double rest = m.share(C::restaurant) / g;
  const double fuel = m.share(C::fuel) / g;
  return rest >= 0.80 && rest <= 1.15 && fuel >= 0.35 && fuel <= 0.60 &&
         groceryLift >= 1.6;
}

void subGateK() {
  using C = pl::merchants::Category;
  const auto &geo = pl::synth::geo::geography();
  constexpr int kPop = 500000;
  const auto catalog = productionCatalog(kPop, true);

  std::vector<double> volume;
  volume.reserve(catalog.records.size());
  for (const auto &r : catalog.records) {
    volume.push_back(r.weight);
  }
  const auto model = commerce::calibrateReachModel(volume, 30.0);
  std::vector<pl::entity::geography::GeoAreaId> homes;
  for (std::size_t a = 1; a <= geo.size(); ++a) {
    homes.push_back(static_cast<pl::entity::geography::GeoAreaId>(a));
  }

  std::printf("\nsub-gate K (category frequency, pop %d with outlets):\n",
              kPop);
  commerce::VisitRateTable ones{};
  ones.fill(1.0);

  for (const int year : {2022, 2005}) {
    const auto sampler = commerce::MembershipSampler::build(
        catalog, geo, homes, model.membership, 30.0,
        commerce::cnpShareForYear(year));
    for (const std::size_t setSize : {std::size_t{30}, std::size_t{19}}) {
      const bool primary = year == 2022 && setSize == 30;
      const auto rows = sampleRows(sampler, primary ? 40000 : 12000, setSize);
      const auto legacy = measureMix(catalog, rows, nullptr);
      const auto ranked =
          measureMix(catalog, rows, &commerce::kVisitRateWeight);

      // K1: the rank multiset, bit for bit.
      std::size_t mismatched = 0;
      std::vector<double> a;
      std::vector<double> b;
      for (const auto &row : rows) {
        rowWeights(catalog, row, nullptr, a);
        rowWeights(catalog, row, &commerce::kVisitRateWeight, b);
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        mismatched += a == b ? 0U : 1U;
      }
      const std::string tag = "year " + std::to_string(year) + " F=" +
                              std::to_string(setSize);
      std::printf("  %s: K1 multiset mismatches %zu of %zu rows, top-1 "
                  "legacy %.4f / ranked %.4f; K2 online visit %.4f -> %.4f\n",
                  tag.c_str(), mismatched, rows.size(), legacy.top1,
                  ranked.top1, legacy.online, ranked.online);
      check(mismatched == 0,
            "K1 " + tag + ": the ranked law must keep each row's legacy rank "
                          "multiset bit for bit");
      check(legacy.top1 == ranked.top1 ||
                std::abs(legacy.top1 - ranked.top1) <= 1e-12,
            "K1 " + tag + ": within-card top-1 must be unchanged");
      check(std::abs(ranked.online - legacy.online) <= 0.02,
            "K2 " + tag + ": the online visit share must stay within 0.02 of "
                          "the dated CNP membership law");

      std::printf("    %-12s %-7s %-7s %s\n", "category", "fav", "legacy",
                  "ranked");
      for (const auto c : pl::merchants::kCategories) {
        const auto i = static_cast<std::size_t>(c);
        const auto label = pl::merchants::name(c);
        std::printf("    %-12.*s %.4f  %.4f  %.4f\n",
                    static_cast<int>(label.size()), label.data(),
                    legacy.fav[i], legacy.visit[i], ranked.visit[i]);
      }
      const double g = ranked.share(C::grocery);
      std::printf("    restaurant/grocery %.3f (DCPC %.3f)  fuel/grocery %.3f "
                  "(DCPC %.3f)  retailOther/grocery %.3f  grocery visit/fav "
                  "%.2f  billers %.4f (legacy %.4f)\n",
                  ranked.share(C::restaurant) / g, kDcpcRestaurantOverGrocery,
                  ranked.share(C::fuel) / g, kDcpcFuelOverGrocery,
                  ranked.share(C::retailOther) / g,
                  g / ranked.fav[static_cast<std::size_t>(C::grocery)],
                  ranked.billers(), legacy.billers());
      if (!primary) {
        continue; // K3-K8 are bounded at the solve point only (era drift is
                  // registered; the 2005 and F=19 splits are printed above)
      }

      // K3 / K5: the DCPC split and its disarm.
      const auto disarm = measureMix(catalog, rows, &ones);
      check(dcpcSplitHolds(ranked),
            "K3: the card-present split must land on the DCPC 2022 ratios "
            "(restaurant/grocery in [0.80, 1.15], fuel/grocery in [0.35, "
            "0.60], grocery visits at least 1.6x its favourite share)");
      check(!dcpcSplitHolds(disarm),
            "K5: with every weight 1 the split must fail K3, or K3 is "
            "vacuous");
      std::printf("  K5 DISARM (all weights 1): restaurant/grocery %.3f, "
                  "fuel/grocery %.3f, grocery visit/fav %.2f\n",
                  disarm.share(C::restaurant) / disarm.share(C::grocery),
                  disarm.share(C::fuel) / disarm.share(C::grocery),
                  disarm.share(C::grocery) /
                      disarm.fav[static_cast<std::size_t>(C::grocery)]);

      // K4: the biller share, and the floor reassignment cannot go below.
      commerce::VisitRateTable last = ones;
      for (const auto c : pl::activity::spending::market::kBillerCategories) {
        last[static_cast<std::size_t>(c)] = 1e-12;
      }
      const auto floorMix = measureMix(catalog, rows, &last);
      std::printf("  K4 biller-category visits %.4f (legacy %.4f); rank floor "
                  "with billers always last %.4f\n",
                  ranked.billers(), legacy.billers(), floorMix.billers());
      check(ranked.billers() <= 0.20,
            "K4: the four biller categories must carry at most 0.20 of "
            "card visits (legacy " +
                std::to_string(legacy.billers()) + ")");

      // The floor value: well below the smallest solved weight, so it reads
      // as "ranked last". 1e-4 against the shipped 1e-3 must move nothing.
      commerce::VisitRateTable deeper = commerce::kVisitRateWeight;
      for (auto &r : deeper) {
        r = r <= 0.001 ? 1e-4 : r;
      }
      const auto deeperMix = measureMix(catalog, rows, &deeper);
      double worstFloorMove = 0.0;
      for (std::size_t i = 0; i < pl::merchants::kCategoryCount; ++i) {
        worstFloorMove = std::max(
            worstFloorMove, std::abs(deeperMix.visit[i] - ranked.visit[i]));
      }
      std::printf("  floor 1e-4 against 1e-3: worst category move %.4f\n",
                  worstFloorMove);
      check(worstFloorMove <= 0.002,
            "K4: the floor must read as 'ranked last'; lowering it tenfold "
            "moved a category share by " +
                std::to_string(worstFloorMove));

      // K6: a property of the set. Reverse each row (swapRemove reorders
      // slots) and compare each merchant's weight; the pick is deterministic.
      std::size_t slotDependent = 0;
      std::size_t nondeterministic = 0;
      for (std::size_t k = 0; k < 2000 && k < rows.size(); ++k) {
        const auto &row = rows[k];
        FavRow reversed{row.person,
                        {row.merchants.rbegin(), row.merchants.rend()}};
        rowWeights(catalog, row, &commerce::kVisitRateWeight, a);
        rowWeights(catalog, reversed, &commerce::kVisitRateWeight, b);
        const auto n = row.merchants.size();
        for (std::size_t i = 0; i < n; ++i) {
          slotDependent += a[i] == b[n - 1 - i] ? 0U : 1U;
        }
        for (const double u : {0.1, 0.5, 0.9}) {
          nondeterministic +=
              commerce::sampleFavoriteSlot(row.merchants, row.person, u,
                                           catalog) ==
                      commerce::sampleFavoriteSlot(row.merchants, row.person,
                                                   u, catalog)
                  ? 0U
                  : 1U;
        }
      }
      check(slotDependent == 0,
            "K6: a merchant's weight must not depend on its slot");
      check(nondeterministic == 0, "K6: the pick must be deterministic");

      // K7: the multiplicative alternative w_c * rank^-0.80, with its weights
      // solved offline to the same DCPC targets on this catalogue
      // (docs/fraud_model_audit.md, outlets-frequency-2026-09).
      const commerce::VisitRateTable multiplicative{
          1.0, 0.4587, 0.04613, 0.05208, 0.3558, 0.8917, 0.1505, 0.5884,
          0.05054, 0.04183};
      double multTop1 = 0.0;
      for (const auto &row : rows) {
        rowWeights(catalog, row, nullptr, a);
        double total = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i) {
          a[i] *= multiplicative[static_cast<std::size_t>(
              catalog.records[row.merchants[i]].category)];
          total += a[i];
        }
        multTop1 += total > 0.0
                        ? *std::max_element(a.begin(), a.end()) / total
                        : 0.0;
      }
      std::printf("  K7 (PRINTED, rejected) multiplicative weights: "
                  "within-card top-1 %.4f against %.4f\n",
                  multTop1 / static_cast<double>(rows.size()), ranked.top1);

      // K10: the fraud venue pool's category factor is this reading. Fraud
      // draws venues from the catalogue by weight, not from a favourite row,
      // so it carries the law as `kCategoryVisitLift`; a table that no longer
      // matches the race would put fraud card rows back on a different
      // category mix from legitimate ones (the fraud/legit category gate in
      // `test_card_merchant_overlap` reads the corpus consequence).
      double worstLiftGap = 0.0;
      double legacyLiftGap = 0.0;
      std::printf("  K10 visit/favourite ratio against kCategoryVisitLift:");
      for (const auto c : pl::merchants::kCategories) {
        const auto i = static_cast<std::size_t>(c);
        const double lift = commerce::kCategoryVisitLift[i];
        const double ratio = ranked.visit[i] / ranked.fav[i];
        worstLiftGap = std::max(worstLiftGap, std::abs(ratio - lift));
        legacyLiftGap = std::max(
            legacyLiftGap, std::abs(legacy.visit[i] / legacy.fav[i] - lift));
        const auto label = pl::merchants::name(c);
        std::printf(" %.*s %.3f/%.2f", static_cast<int>(label.size()),
                    label.data(), ratio, lift);
      }
      std::printf("\n  K10 worst gap %.4f (the legacy law reads %.4f)\n",
                  worstLiftGap, legacyLiftGap);
      check(worstLiftGap <= 0.05,
            "K10: kCategoryVisitLift must be the race's visit-to-favourite "
            "ratio at the solve point (worst gap " +
                std::to_string(worstLiftGap) +
                "); re-read it whenever kVisitRateWeight is re-solved");

      // K8: expected bank payments a year per outlet, at the 236.6 card
      // payments per person-year the 6,000 x 731d acceptance corpus measured
      // (merchant-selection-2026-08). Printed: the research's corrected
      // figure is a few hundred for a typical restaurant or gas outlet, and
      // the gap is registered, not tuned.
      std::vector<double> expected(catalog.records.size(), 0.0);
      for (const auto &row : rows) {
        const double total =
            rowWeights(catalog, row, &commerce::kVisitRateWeight, a);
        for (std::size_t i = 0; i < a.size(); ++i) {
          expected[row.merchants[i]] += a[i] / total;
        }
      }
      const double scale =
          236.6 * static_cast<double>(kPop) / static_cast<double>(rows.size());
      std::printf("  K8 (PRINTED) expected bank payments a year per record:\n");
      for (const auto c : {C::grocery, C::fuel, C::restaurant, C::pharmacy,
                           C::retailOther, C::ecommerce}) {
        std::vector<double> v;
        for (std::size_t m = 0; m < catalog.records.size(); ++m) {
          if (catalog.records[m].category == c &&
              physicalRetail(catalog.records[m]) == (c != C::ecommerce)) {
            v.push_back(expected[m] * scale);
          }
        }
        if (v.empty()) {
          continue;
        }
        std::sort(v.begin(), v.end());
        const auto above = static_cast<std::size_t>(
            v.end() - std::upper_bound(v.begin(), v.end(), 2048.0));
        const auto label = pl::merchants::name(c);
        std::printf("    %-12.*s n %5zu  median %7.0f  p90 %8.0f  max %9.0f  "
                    "above 2,048 %.3f\n",
                    static_cast<int>(label.size()), label.data(), v.size(),
                    v[v.size() / 2],
                    v[static_cast<std::size_t>(0.9 * (v.size() - 1))],
                    v.back(),
                    static_cast<double>(above) /
                        static_cast<double>(v.size()));
      }

      // FMI 2026: 5.4 separate grocery banners a month. Grocery and general
      // merchandise favourites per row, printed: membership is unchanged by
      // this round, so the frequency law imposes no small banner count.
      double banners = 0.0;
      for (const auto &row : rows) {
        for (const auto m : row.merchants) {
          const auto &rec = catalog.records[m];
          banners += physicalRetail(rec) && (rec.category == C::grocery ||
                                             rec.category == C::retailOther)
                         ? 1.0
                         : 0.0;
        }
      }
      std::printf("  grocery + general-merchandise physical favourites per "
                  "row %.2f (FMI: 5.4 grocery banners a month)\n",
                  banners / static_cast<double>(rows.size()));
    }
  }
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
                  "top physical OUTLET spans %zu home areas (its brand %zu)\n",
                  shape.meanMiles, shape.within50, shape.topPhysicalAreas,
                  shape.topPhysicalBrandAreas);
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

  subGateJ();
  subGateK();

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
    // SUB-GATE K9, THE CORPUS PREDICATE PAIRED WITH tests/golden_run.b2sum
    // (cash-hub-defect-2026-08: a digest pins whatever it is given). The
    // card rows themselves must carry the category-dependent frequency:
    // grocery plus restaurant at least 1.5x the four biller categories. The
    // pre-round world put 0.166 against 0.269 on this split at scale
    // (sub-gate K, legacy column), a ratio near 0.6.
    {
      std::map<pl::entity::Key, pl::merchants::Category> categoryOf;
      for (const auto &rec : result.merchants.records) {
        categoryOf.emplace(rec.counterpartyId, rec.category);
      }
      std::size_t cardRows = 0;
      std::size_t everyday = 0;
      std::size_t billers = 0;
      for (const auto &t : result.rows) {
        if (!isCardRail(t) || t.fraud.flag != 0) {
          continue;
        }
        const auto it = categoryOf.find(t.target);
        if (it == categoryOf.end()) {
          continue;
        }
        ++cardRows;
        everyday += it->second == pl::merchants::Category::grocery ||
                            it->second == pl::merchants::Category::restaurant
                        ? 1U
                        : 0U;
        billers +=
            pl::activity::spending::market::isBillerCategory(it->second) ? 1U
                                                                         : 0U;
      }
      const double everydayShare =
          cardRows ? static_cast<double>(everyday) / static_cast<double>(cardRows) : 0.0;
      const double billerShare =
          cardRows ? static_cast<double>(billers) / static_cast<double>(cardRows) : 0.0;
      std::printf("  K9 card rows %zu: grocery+restaurant %.4f, biller "
                  "categories %.4f, ratio %.2f\n",
                  cardRows, everydayShare, billerShare,
                  billerShare > 0.0 ? everydayShare / billerShare : 0.0);
      check(cardRows > 0 && everydayShare >= 1.5 * billerShare,
            std::string(leg.name) +
                ": card rows must put grocery plus restaurant at least 1.5x "
                "the biller categories (the category-frequency law)");
    }

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
    // RE-MEASURED AGAIN (outlets-frequency-2026-09), and the floor holds:
    // armed 2.990 (leg-A) / 2.841 (leg-B) with within-card top-1 0.2040 /
    // 0.2166, against 2.669 / 2.637 and 0.2454 / 0.2352 on the build before.
    // The rank multiset is unchanged (sub-gate K1), so the move is outlets
    // and the category mix reshaping which rows the legs emit.
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
