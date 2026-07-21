#pragma once
//
// phantomledger/synth/geo/catalog.hpp
//
// The geography of the simulated world (geo-causal-v1). PhantomLedger is
// a US retail-banking simulator: customers live in real US places
// (population-weighted) and their everyday spending is domestic.
// International cities are the destinations of realistic cross-border
// events — travel and online / stolen-card fraud abroad — which the
// transaction model reaches in G2.
//
// The city data lives in a pinned DATA FILE (data/geo/us_cities.csv),
// NOT hardcoded in source and NOT selected by any CLI flag: the path is
// fixed at build time (PL_GEO_DATA, a compile definition, like the
// tests' PL_BIN_PATH). `geography()` loads it once and returns a stable
// reference; homes, merchant outlets, and the exporters all read the
// SAME rows, so a person's home, the merchant they visit, and the
// geography reported downstream are the one coherent (city, state,
// postal, coordinates) tuple — never independently synthesized.
//

#include "phantomledger/entities/geography/area.hpp"

namespace PhantomLedger::synth::geo {

// The world catalogue: US places first (1-based ids, home + domestic
// commerce), then major international cities. Loaded once from the
// pinned data file, immutable, process-wide.
[[nodiscard]] const entity::geography::GeoCatalog &geography();

} // namespace PhantomLedger::synth::geo
