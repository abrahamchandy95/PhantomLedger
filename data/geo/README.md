# Canonical geographic catalogue (`geo-causal-v1`)

This directory holds the **pinned, normalized geographic artifact** that
PhantomLedger's world model references for *all* geography: person and
household homes, merchant outlet locations, and the card-fraud graph's
`City` / `State` / `Zipcode` vertices. Every one of those resolves to a
row here, so the geography a corpus reports is causally the same data
the corpus was generated from — never invented at export time.

The loader is `synth::geo::loadGeoCatalog`
([include/phantomledger/synth/geo/catalog.hpp](../../include/phantomledger/synth/geo/catalog.hpp));
the row type is `entity::geography::GeoArea`
([include/phantomledger/entities/geography/area.hpp](../../include/phantomledger/entities/geography/area.hpp)).

## Production vs test

- **Production** (`--usecase card-fraud|standard|aml|aml-txn-edges|mule-ml`)
  REQUIRES a real pinned catalogue, passed as `--geo-catalog data/geo/us_geo_v1.csv`.
  A missing/broken catalogue is **fatal** — there is deliberately **no**
  silent random fallback in production (the old `synthesizeUsZipTable`
  fallback produced incoherent city/ZIP pairs and no coordinates; it is
  retired for corpus generation). *(The fatal wiring lands in round G1,
  when the world build first consumes the catalogue.)*
- **Tests / smoke runs** use the tiny deterministic fixture
  [../../tests/fixtures/geo/us_geo_fixture.csv](../../tests/fixtures/geo/us_geo_fixture.csv)
  — 16 rows, enough to exercise the loader and the geo model. Never use
  the fixture for a published corpus.

The large `us_geo_v1.csv` itself is **not** committed (it is a build
artifact of the sources below). The **manifest is committed**
(`us_geo_v1.manifest.json`) and pins the vintages + source hashes so the
artifact is reproducible and a run's geography is auditable.

## CSV contract

UTF-8, comma-separated, one **header row** naming the columns (order is
free — the loader maps by name), `#` line comments and blank lines
ignored, **no embedded commas or quotes**. Required columns:

| column | meaning |
|---|---|
| `postal_area_code` | ZCTA / postal-area code (string; NOT a USPS ZIP route) |
| `city` | place name |
| `state_code` | 2-letter state/admin code, e.g. `NY` |
| `state_name` | full state/admin name |
| `country` | ISO-ish 2-letter code matching `locale::code` (`US`, `GB`, …) |
| `latitude_e6` | latitude in **integer microdegrees** (deg × 1e6) |
| `longitude_e6` | longitude in **integer microdegrees** |
| `population` | ACS residential population (unsigned) |
| `land_area_km2` | Census land area, km² (unsigned) |

**Row order defines the 1-based `GeoAreaId`**, so row order is part of
what the manifest pins — do not reshuffle without re-pinning.

## Building the artifact (offline)

Join, by ZCTA, three pinned public sources into one normalized CSV:

1. **Census ZCTA Gazetteer** — area identifier, land area, and the
   representative (internal-point) latitude/longitude. Convert lat/long
   to microdegrees (`round(deg * 1e6)`).
2. **ACS 5-year** population estimates by ZCTA → `population`.
3. **GeoNames postal-code data** — postal place / administrative names
   where the Gazetteer lacks a clean city/state label (attribution
   licence; record the snapshot date).

**Pin the vintage to the experiment, do not use "latest".** For a
TabFormer-era (≈1991–2019) benchmark, pin a Gazetteer/ACS vintage
compatible with the experiment (likely 2017–2018) and record it.

### Semantics — do not overclaim

A ZCTA is a **generalized** area representation; it is **not** a literal
USPS ZIP delivery route, and not every ZIP has a ZCTA. Internally the
model uses `PostalArea` / `GeoArea`; the TigerGraph vertex retains the
name **`Zipcode`** only for schema compatibility, and its value is the
`postal_area_code`. Document this wherever the corpus is published.

## Manifest

`us_geo_v1.manifest.json` records the model name, the pinned source
vintages, the normalized-schema version, and the SHA-256 of each source
input. A run is reproducible from (manifest + seed); two runs with the
same manifest and seed are byte-identical.
