//
// tests/test_geo_selection.cpp
//
// geo-causal-v1 (G2a step-2) ACCEPTANCE GATES — the MODEL, not the bytes.
// The golden digests pin the corpus byte-for-byte but say nothing about
// whether the geography behaves as intended. These gates assert the causal
// claims directly, serverless and deterministic (even-u sampling of the
// GeographicMerchantPools CDFs — no PostgreSQL, no rng-stream dependency):
//
//   1. DISTANCE DECAY   card-present picks concentrate on merchants near the
//                       customer's home (a local outlet dominates a distant
//                       one of equal popularity).
//   2. LOCATION-VARYING the decay scale falls with the home area's urbanicity
//                       (NYC ~4 mi < Denver < Burlington ~50 mi).
//   3. ONLINE-FREE      online / non-physical merchants are EXCLUDED from the
//                       card-present pool regardless of popularity (reached
//                       only through the geography-free national CDF).
//   4. EMERGENCE        an equal-popularity merchant in a big city earns more
//                       realized volume than one in a small town, purely from
//                       nearby-customer count (axiom 7) — success EMERGES.
//
// These validate DIRECTION, not calibrated magnitude; true calibration
// (distance distributions, share, decay curve) is geo debt (see systemprompt).
//

#include "phantomledger/activity/spending/market/commerce/local_pools.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace commerce = ::PhantomLedger::activity::spending::market::commerce;
namespace geo = ::PhantomLedger::entity::geography;
namespace merch = ::PhantomLedger::entity::merchant;
namespace merchcat = ::PhantomLedger::merchants;
namespace synthgeo = ::PhantomLedger::synth::geo;
namespace locale = ::PhantomLedger::locale;

namespace {

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// merchant-selection-2026-08: `LocalPools` takes the law to localise as an
// explicit weight vector, where the retired `GeographicMerchantPools` read
// `Record.weight` implicitly. Passing the volume weights reproduces the old
// behaviour exactly, which is what keeps this gate's bands comparable.
[[nodiscard]] std::vector<double>
weightsOf(const PhantomLedger::entity::merchant::Catalog &catalog) {
  std::vector<double> out;
  out.reserve(catalog.records.size());
  for (const auto &rec : catalog.records) {
    out.push_back(rec.weight);
  }
  return out;
}

[[nodiscard]] geo::GeoAreaId findCity(const geo::GeoCatalog &cat,
                                      std::string_view city) {
  for (const auto &a : cat.areas()) {
    if (a.country == locale::Country::us && a.city == city) {
      return a.id;
    }
  }
  return geo::invalidGeoArea;
}

[[nodiscard]] merch::Record physicalMerchant(geo::GeoAreaId loc,
                                             double weight) {
  merch::Record r;
  r.category = merchcat::Category::grocery;
  r.weight = weight;
  r.location = loc;
  r.footprint = merch::Footprint::localOutlet;
  return r;
}

[[nodiscard]] merch::Record onlineMerchant(double weight) {
  merch::Record r;
  r.category = merchcat::Category::ecommerce;
  r.weight = weight;
  r.location = geo::invalidGeoArea;
  r.footprint = merch::Footprint::online;
  return r;
}

// Deterministic proportion of pool picks landing on each catalog index, via
// evenly-spaced u in (0,1) — this reproduces the CDF mass without any rng.
struct PickTally {
  std::vector<std::uint32_t> counts; // by catalog record index
  int samples = 0;
};

[[nodiscard]] PickTally tally(const commerce::LocalPools &pools,
                              geo::GeoAreaId home, std::size_t catalogSize,
                              int n) {
  PickTally out;
  out.counts.assign(catalogSize, 0);
  out.samples = n;
  for (int i = 0; i < n; ++i) {
    const double u = (static_cast<double>(i) + 0.5) / static_cast<double>(n);
    out.counts[pools.sample(home, u)] += 1;
  }
  return out;
}

} // namespace

int main() {
  const auto &cat = synthgeo::geography();

  const auto nyc = findCity(cat, "New York");
  const auto la = findCity(cat, "Los Angeles");
  const auto denver = findCity(cat, "Denver");
  const auto burlington = findCity(cat, "Burlington");

  check(geo::validArea(nyc) && geo::validArea(la) && geo::validArea(denver) &&
            geo::validArea(burlington),
        "fixture cities (New York, Los Angeles, Denver, Burlington) resolve");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed (fixture)\n", g_failures);
    return 1;
  }

  // Gate 1 — DISTANCE DECAY. Two equal-popularity physical merchants: one in
  // NYC, one in LA. A New Yorker's card-present picks should overwhelmingly
  // land on the local (NYC) outlet.
  {
    merch::Catalog catalog;
    catalog.records.push_back(physicalMerchant(nyc, 1.0)); // index 0 (local)
    catalog.records.push_back(physicalMerchant(la, 1.0));  // index 1 (distant)

    const std::vector<geo::GeoAreaId> homes{nyc};
    const auto pools =
        commerce::LocalPools::build(catalog, cat, homes, weightsOf(catalog));

    check(pools.has(nyc), "gate1: NYC home has a distance-decay pool");
    const auto t = tally(pools, nyc, catalog.records.size(), 1000);
    check(t.counts[0] >= static_cast<std::uint32_t>(t.samples * 0.99),
          "gate1: NYC-home card-present strongly prefers the local NYC "
          "merchant over the LA one (>=99%), got " +
              std::to_string(t.counts[0]) + "/" + std::to_string(t.samples));
  }

  // Gate 2 — LOCATION-VARYING DECAY. Scale falls with the home area's
  // population (urbanicity proxy): dense NYC decays fast, small Burlington
  // slow, Denver in between.
  {
    const double sNyc = commerce::decayScaleMilesFor(cat.at(nyc));
    const double sDen = commerce::decayScaleMilesFor(cat.at(denver));
    const double sBur = commerce::decayScaleMilesFor(cat.at(burlington));

    check(sNyc <= 4.0 + 1e-9,
          "gate2: dense NYC decays at the urban floor (~4 mi), got " +
              std::to_string(sNyc));
    check(sBur >= 50.0 - 1e-9,
          "gate2: small Burlington decays at the small-town ceiling (~50 mi), "
          "got " +
              std::to_string(sBur));
    check(sNyc < sDen && sDen < sBur,
          "gate2: decay scale decreases with population "
          "(NYC < Denver < Burlington)");
  }

  // Gate 3 — ONLINE IS GEOGRAPHY-FREE. An online merchant with HIGHER
  // popularity is still never drawn from the card-present pool: it has no
  // physical location and is reached only through the national CDF.
  {
    merch::Catalog catalog;
    catalog.records.push_back(physicalMerchant(nyc, 1.0)); // index 0 physical
    catalog.records.push_back(onlineMerchant(5.0));        // index 1 online

    const std::vector<geo::GeoAreaId> homes{nyc};
    const auto pools =
        commerce::LocalPools::build(catalog, cat, homes, weightsOf(catalog));

    check(pools.has(nyc), "gate3: NYC home has a pool with the physical outlet");
    const auto t = tally(pools, nyc, catalog.records.size(), 500);
    check(t.counts[1] == 0,
          "gate3: the online merchant is excluded from the card-present pool "
          "despite higher weight, got " +
              std::to_string(t.counts[1]) + " online picks");
    check(t.counts[0] == static_cast<std::uint32_t>(t.samples),
          "gate3: every card-present pick is the physical outlet");
  }

  // Gate 4 — EMERGENCE. Two equal-popularity merchants: one in the most
  // populous area (NYC), one in the least (Burlington). Sum realized volume
  // over population-weighted US homes. The big-city merchant wins by a wide
  // margin — purely because more customers live near it.
  {
    merch::Catalog catalog;
    catalog.records.push_back(physicalMerchant(nyc, 1.0));        // 0 big city
    catalog.records.push_back(physicalMerchant(burlington, 1.0)); // 1 small town

    std::vector<geo::GeoAreaId> homes;
    for (const auto &a : cat.areas()) {
      if (a.country == locale::Country::us) {
        homes.push_back(a.id);
      }
    }

    const auto pools =
        commerce::LocalPools::build(catalog, cat, homes, weightsOf(catalog));

    double volBig = 0.0;
    double volSmall = 0.0;
    for (const auto &a : cat.areas()) {
      if (a.country != locale::Country::us || !pools.has(a.id)) {
        continue;
      }
      const auto t = tally(pools, a.id, catalog.records.size(), 400);
      const double pop = static_cast<double>(a.population);
      volBig += pop * (static_cast<double>(t.counts[0]) / t.samples);
      volSmall += pop * (static_cast<double>(t.counts[1]) / t.samples);
    }

    check(volBig > 2.0 * volSmall,
          "gate4: equal-popularity big-city merchant earns more realized "
          "volume than the small-town one (emergence)");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_geo_selection: all gates passed\n");
  return 0;
}
