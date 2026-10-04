# Geographic catalogue (`geo-causal-v1`)

The simulator's geography: real US places (homes and domestic commerce,
population-weighted) plus major international cities (travel and
cross-border fraud destinations, and homes for the small non-US locale
share); 86 rows, 71 US and 15 international. Homes, merchant outlets and
every exporter read the same rows, so a person's home, the merchants they
visit and the reported geography form one `(city, state, postal,
coordinates)` tuple.

## How it is loaded

The rows are embedded as constexpr data in
`include/phantomledger/synth/geo/geo_data.hpp`, transcribed verbatim from
`us_cities.csv` (owner directive to minimize repo data files,
2026-07-24). `synth::geo::geography()`
([src/synth/geo/catalog.cpp](../../src/synth/geo/catalog.cpp)) builds the
catalogue once and caches it. Generation does not read this CSV, and there
is no CLI flag or path setting. Changing geography means editing
`geo_data.hpp` in a named model-moving round: append rows, never reorder.

## Columns

UTF-8 CSV, one header row, `#` comments and blank lines allowed, no
embedded commas. Row order defines the 1-based `GeoAreaId`.

| column | meaning |
|---|---|
| `country` | 2-letter code matching `locale::code` (`US`, `GB`, …) |
| `postal_area_code` | representative postal code (string; not a USPS route) |
| `city` | place name |
| `state_code` | state or first-level region code (`NY`, `ON`, …) |
| `state_name` | full state or region name |
| `latitude_e6` | latitude in integer microdegrees (deg × 1e6) |
| `longitude_e6` | longitude in integer microdegrees (US negative) |
| `population` | approximate municipal population (unsigned) |

Populations and coordinates are approximate real values [Likely; verify
at citation time]; order-of-magnitude accuracy suffices for
population-weighted home placement.
