#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace PhantomLedger::entity::merchant {

struct Label {
  std::uint64_t value = 0;

  friend constexpr bool operator==(const Label &,
                                   const Label &) noexcept = default;
};

// MERCHANT LIFECYCLE (merchant-churn-2026-07).
//
// A HALF-OPEN operating interval, `[firstEpoch, lastEpochExcl)`, matching
// `infra::Tenure`'s convention so the two liveness models read the same way
// (and neither can double-count a boundary instant).
//
// WHY THIS EXISTS. `makeCatalog` took no window and no date, and
// `merchant::Record` carried no time field of any kind, so **every merchant
// in the catalogue was live for every day of the run.** Over the owner's
// 20-year target window that is a claim no retail geography supports: BLS
// Business Employment Dynamics puts retail 1-year survival near 84% and
// 5-year near 58%, and the March-1994 birth cohort was down to ~14% by
// March 2025. A static catalogue does not just lose realism — it deletes
// the entire "merchant closed / merchant is new" axis, which is exactly
// where a fraud model expects elevated risk.
//
// THE DEFAULT IS DELIBERATELY ALWAYS-LIVE. An unassigned record answers
// true to `liveAt` for every timestamp, so a caller that never runs
// `assignLifecycle` (every existing unit test, and any harness that builds
// a catalogue directly) sees the pre-lifecycle behaviour unchanged. Making
// the default a closed interval would have turned dozens of tests red for
// reasons unrelated to what they assert.
inline constexpr std::int64_t kEpochMin =
    std::numeric_limits<std::int64_t>::min();
inline constexpr std::int64_t kEpochMax =
    std::numeric_limits<std::int64_t>::max();

// A merchant's commercial reach (geo-causal-v1). A Merchant record is a
// merchant ACCEPTANCE LOCATION / OUTLET, not an abstract nationwide
// brand — so a physical outlet has a real GeoArea and online commerce
// is explicitly geography-free. Footprint (with category) governs which
// customers can plausibly reach it during causal selection (G2); it is
// NOT a fake "local HQ" for an online-only merchant.
enum class Footprint : std::uint8_t {
  localOutlet,     // reached mostly by nearby residents (grocery, fuel, …)
  regionalOutlet,  // metro/region reach (larger retail, some healthcare)
  nationalService, // service billed nationwide (telecom, insurance)
  online,          // online card use — the card IS used, just remotely
                   // (the "Online" use_chip value); distance does not apply
};

struct Record {
  Label label;
  entity::Key counterpartyId;
  ::PhantomLedger::merchants::Category category;
  double weight = 0.0;

  // geo-causal-v1: the outlet's canonical location and reach. Both are
  // ASSIGNED during merchant synthesis (G1c, synth::merchants::
  // placeGeography) from the pinned geo catalogue on dedicated per-merchant
  // RNG lanes — a physical outlet gets a population-weighted US area, an
  // `online` outlet keeps `location == invalidGeoArea` (no physical
  // geography — reachable from anywhere). READ by the card-fraud merchant
  // exporter, which resolves `location` through `synth::geo::geography()`
  // (the acausal geoIndexFor is gone).
  entity::geography::GeoAreaId location = entity::geography::invalidGeoArea;
  Footprint footprint = Footprint::localOutlet;

  // merchant-ownership-2026-07: the PROPRIETOR PARTY the institution
  // holds a beneficial-owner record for, or `invalidPerson` when it holds
  // none (the majority — most of an acceptance catalog is other banks'
  // merchants and corporately owned chains).
  //
  // Filled in the entity stage from the existing business-owner cohort by
  // `entity::merchant::ownership::ownerFor`, which is DRAW-FREE and reads
  // the merchant KEY alone. Both properties are load-bearing: draw-free
  // means adding this moved no corpus byte, and key-only means owner-edge
  // presence cannot correlate with footprint — which matters because
  // fraud destination selection IS footprint-conditioned, so an
  // eligibility rule that read `footprint` or `weight` would have turned
  // this ownership register into a fraud shortcut. See the resolver's
  // header for that argument in full.
  //
  // EXPORTED as `cf_Is_Merchant`, whose emptiness hard-aborts the
  // downstream `tf_gnn_loader_v2` push.
  entity::PersonId owner = entity::invalidPerson;

  // merchant-churn-2026-07: the operating interval, half-open. Assigned by
  // `synth::merchants::assignLifecycle` on an isolated per-merchant lane,
  // the same corpus-neutral pattern `placeGeography` uses. See the
  // kEpochMin/kEpochMax note above for why the default is always-live.
  std::int64_t firstEpoch = kEpochMin;
  std::int64_t lastEpochExcl = kEpochMax;

  [[nodiscard]] constexpr bool liveAt(std::int64_t ts) const noexcept {
    return ts >= firstEpoch && ts < lastEpochExcl;
  }

  // True when the record carries a real interval rather than the
  // always-live default. Lets a gate distinguish "lifecycle was never
  // assigned" from "assigned and this merchant happens to span the
  // window" — a distinction the liveness predicate alone cannot make, and
  // the one a vacuous-gate check needs.
  [[nodiscard]] constexpr bool lifecycleAssigned() const noexcept {
    return firstEpoch != kEpochMin || lastEpochExcl != kEpochMax;
  }
};

struct Catalog {
  std::vector<Record> records;

  // Count of records live at `ts`. O(n) and intended for setup and gates,
  // never for per-transaction selection — the hot path reads the monthly
  // live CDF the commerce evolver rebuilds.
  [[nodiscard]] std::size_t liveCountAt(std::int64_t ts) const noexcept {
    std::size_t live = 0;
    for (const auto &record : records) {
      if (record.liveAt(ts)) {
        ++live;
      }
    }
    return live;
  }
};

} // namespace PhantomLedger::entity::merchant
