#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entity::merchant {

struct Label {
  std::uint64_t value = 0;

  friend constexpr bool operator==(const Label &,
                                   const Label &) noexcept = default;
};

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
};

struct Catalog {
  std::vector<Record> records;
};

} // namespace PhantomLedger::entity::merchant
