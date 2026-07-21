#pragma once
//
// phantomledger/synth/geo/catalog.hpp
//
// The compiled-in geography of the simulated world (geo-causal-v1).
// PhantomLedger is a US retail-banking simulator: customers live in
// real US places (population-weighted), and their everyday spending is
// domestic. International cities are included as the destinations of
// realistic cross-border events — travel and card-not-present / stolen-
// card fraud abroad — which the transaction model reaches in G2.
//
// Geography is EMBEDDED, not loaded: no CLI flag, no external file, no
// manifest. Same discipline as the locale/us_state + locale/us_cities
// tables — reference data lives in the binary. `geography()` builds the
// catalogue once from the embedded rows and returns a stable reference;
// homes, merchant outlets, and the exporters all read the SAME rows, so
// a person's home, the merchant they visit, and the geography reported
// downstream are the one coherent (city, state, postal, coordinates)
// tuple — never independently synthesized.
//

#include "phantomledger/entities/geography/area.hpp"

namespace PhantomLedger::synth::geo {

// The world catalogue: US places first (1-based ids, home + domestic
// commerce), then major international cities (travel / cross-border
// fraud destinations). Built once, immutable, process-wide.
[[nodiscard]] const entity::geography::GeoCatalog &geography();

} // namespace PhantomLedger::synth::geo
