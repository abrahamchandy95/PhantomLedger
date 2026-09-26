#pragma once
//
// phantomledger/synth/merchants/outlets.hpp
//
// CHAIN OUTLETS (outlets-frequency-2026-09). A catalogue Record was already an
// acceptance endpoint (merchants.hpp), but a physical chain was ONE endpoint
// in ONE of the 71 city rows: the largest grocery brand took its whole volume
// weight through a single counterparty in a single area. Issuers see a chain
// one outlet at a time (Visa Merchant Data Standards Manual, April 2026: the
// acquirer assigns each merchant outlet its own location; card-absent
// merchants use one principal place of business). What was missing is the
// GROUPING, so this expands each physical chain brand into outlet records
// that share a `Record::brand`.
//
// THE RULE, DERIVED FROM THE CATALOGUE AND NOT TUNED. An eligible brand of
// weight w becomes n = max(1, round(w / unit)) outlets, where unit is the
// MEDIAN weight of the eligible core records: a chain is a brand whose volume
// would take several typical stores to carry. The brand weight is split
// equally over its outlets, so every volume law reads the same totals. The
// rule has no free constant, and it was checked (not fitted) against SUSB
// 2022 Retail Trade, where firms with 500+ employees hold 63.2% of receipts
// and 31.3% of establishments. `test_card_merchant_graph` sub-gate J bounds
// the chain shares around those two figures at pop 500,000.
//
// ELIGIBLE: core records (serial <= coreCount; the tail is independents,
// SUSB's 1.12 establishments per sub-500 firm) in the five card-present
// retail categories, with a local or regional footprint and a placed area.
// NOT ELIGIBLE, each for a reason:
//   * online records and ecommerce: one principal place of business;
//   * nationalService records: billed nationally, no storefronts to count;
//   * the four biller categories: a utility or insurer is paid as one
//     account wherever the payer lives.
//
// PLACEMENT. The n - 1 added outlets take population-weighted US areas drawn
// i.i.d. with replacement (the same law `placeGeography` uses), so a big
// city can hold several outlets of one brand. No regional clustering is
// modelled; that is registered in docs/fraud_model_audit.md.
//
// STORED, NOT DERIVED, and that is the RAM rule applied rather than broken
// (docs/ram_derive_dont_store.md). Every sampler, the favourite CSR and the
// ledger address merchants by catalogue index, and every endpoint must be a
// registered account, so an outlet has to be a Record. What is derived is the
// PLAN: the count comes from the weights and the areas from a per-brand lane,
// so the catalogue replays from the seed. The cost is O(outlets), about 13%
// more records at pop 500,000.
//
// DRAW DISCIPLINE. No draw on the shared entity stream (`makeCatalog`'s draw
// count is untouched). One lane per expanded brand, {"merchant-outlet",
// serial} on the geo seed, spending n - 1 uniforms: the count depends on
// data, and it is the only consumer of its lane, so it is last on it.
//
// OWNERSHIP STAYS KEY-ONLY. Each outlet has its own counterparty key and
// takes its proprietor from `ownership::ownerFor` on that key, the franchisee
// reading. Keying the owner on the brand would make one Party the owner of a
// 50-outlet chain; making chain outlets ownerless would correlate ownership
// with footprint, the leak `test_card_endpoint_graph` sub-gate G'' catches.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/synth/merchants/place.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::merchants {

inline constexpr std::array<::PhantomLedger::merchants::Category, 5>
    kOutletCategories{
        ::PhantomLedger::merchants::Category::grocery,
        ::PhantomLedger::merchants::Category::fuel,
        ::PhantomLedger::merchants::Category::restaurant,
        ::PhantomLedger::merchants::Category::pharmacy,
        ::PhantomLedger::merchants::Category::retailOther,
    };

[[nodiscard]] constexpr bool
isOutletCategory(::PhantomLedger::merchants::Category category) noexcept {
  return std::ranges::find(kOutletCategories, category) !=
         kOutletCategories.end();
}

[[nodiscard]] inline bool
isOutletEligible(const entity::merchant::Record &rec, int coreCount) noexcept {
  using F = entity::merchant::Footprint;
  return coreCount > 0 &&
         rec.label.value <= static_cast<std::uint64_t>(coreCount) &&
         isOutletCategory(rec.category) &&
         (rec.footprint == F::localOutlet ||
          rec.footprint == F::regionalOutlet) &&
         entity::geography::validArea(rec.location);
}

// The median weight of the eligible core records, or 0 when there are none.
// Draw-free. The upper median for an even count, so it is always a weight
// some record really has.
[[nodiscard]] inline double
outletUnitWeight(const entity::merchant::Catalog &catalog, int coreCount) {
  std::vector<double> weights;
  for (const auto &rec : catalog.records) {
    if (isOutletEligible(rec, coreCount) && rec.weight > 0.0) {
      weights.push_back(rec.weight);
    }
  }
  if (weights.empty()) {
    return 0.0;
  }
  const auto mid = weights.begin() +
                   static_cast<std::ptrdiff_t>(weights.size() / 2);
  std::nth_element(weights.begin(), mid, weights.end());
  return *mid;
}

// Outlets a brand of `weight` is expanded into under `unit`. One when the
// unit is unusable, so a degenerate catalogue expands nothing.
[[nodiscard]] inline std::size_t outletCountFor(double weight,
                                                double unit) noexcept {
  if (!(unit > 0.0) || !std::isfinite(unit) || !(weight > 0.0)) {
    return 1;
  }
  return static_cast<std::size_t>(
      std::max(1LL, std::llround(weight / unit)));
}

// Expand every eligible chain brand into outlets. Must run AFTER
// `placeGeography` (it reads footprint and location, and the added outlets
// are placed here rather than there) and BEFORE `appendChurnReplacements`
// (replacements draw donors over every establishment, outlets included).
inline void expandOutlets(entity::merchant::Catalog &catalog, int coreCount,
                          std::uint64_t outletSeed) {
  const double unit = outletUnitWeight(catalog, coreCount);
  if (!(unit > 0.0)) {
    return;
  }
  const auto areas = detail::domesticAreas();
  if (areas.ids.empty()) {
    return;
  }

  const random::RngFactory factory{outletSeed};
  const auto baseCount = catalog.records.size();

  std::uint64_t nextSerial = 0;
  for (const auto &rec : catalog.records) {
    nextSerial = std::max(nextSerial, rec.label.value);
  }
  ++nextSerial;

  for (std::size_t i = 0; i < baseCount; ++i) {
    if (!isOutletEligible(catalog.records[i], coreCount)) {
      continue;
    }
    const auto n = outletCountFor(catalog.records[i].weight, unit);
    if (n <= 1) {
      continue;
    }

    auto &brandRec = catalog.records[i];
    const double share = brandRec.weight / static_cast<double>(n);
    brandRec.brand = brandRec.label;
    brandRec.weight = share;
    // Copied, because the push_back below may reallocate.
    const auto brand = brandRec;

    std::array<char, 20> buf{};
    auto lane = factory.rng(
        {"merchant-outlet", detail::serialView(buf, brand.label.value)});

    for (std::size_t k = 1; k < n; ++k) {
      const auto idx = dist::sampleIndex(areas.cdf, lane.nextDouble());

      entity::merchant::Record outlet{};
      outlet.label = entity::merchant::Label{nextSerial};
      outlet.counterpartyId =
          entity::makeKey(brand.counterpartyId.role, brand.counterpartyId.bank,
                          nextSerial);
      outlet.category = brand.category;
      outlet.weight = share;
      outlet.location = areas.ids[idx];
      outlet.footprint = brand.footprint;
      outlet.brand = brand.label;
      catalog.records.push_back(outlet);
      ++nextSerial;
    }
  }
}

} // namespace PhantomLedger::synth::merchants
