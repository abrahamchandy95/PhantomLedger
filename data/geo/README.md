# Geographic catalogue (`geo-causal-v1`)

`us_cities.csv` is the pinned geographic catalogue for the whole
simulator: real US places (homes + domestic commerce, population-
weighted) plus major international cities (travel / cross-border-fraud
destinations, and homes for the small non-US locale share). Homes,
merchant outlets, and every exporter read the SAME rows, so a person's
home, the merchant they visit, and the geography reported downstream are
one coherent `(city, state, postal, coordinates)` tuple.

## How it is loaded — no flag, no runtime choice

The file path is fixed at **build time** as the compile definition
`PL_GEO_DATA` (see `CMakeLists.txt`, on the `pl_world` target) — exactly
like the tests' `PL_BIN_PATH`. `synth::geo::geography()`
([src/synth/geo/catalog.cpp](../../src/synth/geo/catalog.cpp)) loads it
once and caches it. There is **no `--geo-catalog` CLI flag and no
runtime path selection** — the CLI surface is fixed. Editing the model's
geography means editing this file (or pointing `PL_GEO_DATA` at another
copy at build time), not passing an argument.

## CSV format

UTF-8, comma-separated, one header row (columns mapped by name, order
free), `#` line comments and blank lines ignored, no embedded commas.
**Row order defines the 1-based `GeoAreaId`.** Required columns:

| column | meaning |
|---|---|
| `country` | 2-letter code matching `locale::code` (`US`, `GB`, …) |
| `postal_area_code` | representative postal code (string; not a USPS route) |
| `city` | place name |
| `state_code` | state / first-level region code (`NY`, `ON`, …) |
| `state_name` | full state / region name |
| `latitude_e6` | latitude in integer microdegrees (deg × 1e6) |
| `longitude_e6` | longitude in integer microdegrees (US = negative) |
| `population` | approximate municipal population (unsigned) |

Populations/coordinates are approximate real values [Likely — verify at
citation time]; they need only be order-of-magnitude correct for
population-weighted home placement. The set is easily extended — add
rows (any populated US market or international destination).
