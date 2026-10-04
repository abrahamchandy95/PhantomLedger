# Era reference data: provenance and refresh contract (macro-history-v1)

The 1990-2024 US era series, the mortality table and the calibration year
are constexpr tables in `include/phantomledger/synth/econ/era_data.hpp`,
which replaced the retired `data/econ/*.csv` files (owner directive to
minimize repo data files, 2026-07-24). They are typed and validated by
`synth::econ::{macroSeries(),mortality()}` (`src/synth/econ/catalog.cpp`),
gated by `tests/test_econ_catalog.cpp`, and exported to PostgreSQL for
every use case as `econ.macro_annual`, `econ.mortality` and
`econ.provenance` (exporter::econ, pinned by `tests/test_econ_tables.cpp`).
Generation reads them since the H1 to H4 rounds; the app-layer H0.6
card-fraud era lock reads the coverage bounds.

## The calibration year (H1 anchor): owner-approved CHOICE

`kCalibrationYear = 2019` is the year the calibrated dollar constants are
denominated in. They were measured roughly 2015-2024; the CHOICE
(macro-history-2026-07c) declares them 2019 dollars, the last full year of
the canonical card-fraud window and the last pre-COVID year. H1 consumers
scale by `index(year) / index(kCalibrationYear)` (AWI for incomes, CPI for
prices).

Design rule (the owner's 2050 criterion): the calibration year is a
provenance fact of the calibration data, never "the present", the last
covered year, the wall clock or the run start. It lives in the pinned data
(validated inside coverage) and changes only with the constants it
denominates, in a named model-moving round. Rejected, and staying rejected:
the latest coverage year (a data refresh would silently re-denominate the
economy; coverage is not calibration) and wall-clock or run-start anchoring
(breaks determinism and comparability). Per-constant measurement vintages
are the registered upgrade.

## kMacroAnnual: one row per year, 1990-2024 contiguous

Integer-encoded; the builder never parses floats.

| Field | Meaning, encoding | Source | Status |
|---|---|---|---|
| `year` | calendar year | | |
| `cpiUE3` | CPI-U annual average, US city average (1982-84=100), × 1000 | BLS CUUR0000SA0 == FRED `CPIAUCNS` (monthly NSA) | Verified exact 2026-07-24 as the mean of 12 NSA months, to the third decimal; H1 replaced tenths-rounded 1990-2006 values |
| `awiCents` | SSA Average Wage Index, dollars × 100 | ssa.gov/oact/cola/awiseries.html | Verified exact 2026-07-24. Year N publishes about October N+1: the binding coverage limit |
| `pceDollars` | nominal per-capita PCE, exact dollars | BEA via FRED `A794RC0A052NBEA` | Verified exact, vintage 2026-04-09 |
| `unempBp` | U-3 annual average, percent × 100 | BLS LNS14000000 | Within 0.1pp of FRED `UNRATENSA` monthly means (2026-07-24); a few years differ in the last digit (2011, 2021) because the official figure is a ratio of annual averages |
| `recessionMonths` | NBER recession months in the year, 0-12 | NBER dating | dates [Certain] |
| `populationThousands` | BEA NIPA midperiod population, thousands | FRED `B230RC0A052NBEA` | Verified exact, vintage 2026-02-20. The per-capita PCE denominator, not Census July-1 (<0.3% apart; never mix) |

Axis notes:

- Unemployment is the annual average; monthly peaks were 7.8% (1992-06),
  6.3% (2003-06), 10.0% (2009-10), 14.7% (2020-04). Monthly modeling
  needs a monthly series in its own round.
- `recessionMonths` counts months after the peak month through the
  trough, so years sum to published durations: 1990:5 + 1991:3 = 8;
  2001:8; 2008:12 + 2009:6 = 18; 2020:2; 2021-2024 zero.
- The canonical window `[1991-01-01, 2020-01-01)` ends before COVID. The
  H0.6 lock accepts 2020-2024 windows; H4 wires the 2020-21 consumption
  swing, but EIP checks and the 2020 saving spike are unmodeled.
- Pinned in test_econ_catalog: CPI 2019/1991 about 1.88; AWI about 2.48;
  per-capita PCE 2019/1990 = 43,682/15,225, about 2.87; 2009 is the only
  CPI deflation and AWI dip; 2020 is the only per-capita PCE dip, with
  8.1% unemployment; 2021 to 2022 is the largest CPI jump (about 8.0%);
  population rises every year (slowest 2021).

## Why coverage ends at 2024 (as of 2026-07)

A row needs every column fully measured: no projections, partial years or
mixed cells. 2025 fails three ways: AWI 2025 publishes about October 2026;
the October 2025 CPI release was cancelled (federal shutdown), so no
official 2025 CPI average exists; the October 2025 CPS survey was never
collected, so no unemployment average either. The measured frontier
always lags the present. The engine never hardcodes it: the builder
requires first ≤ 1990 and last ≥ 2020, and consumers (the H0.6 lock, H1
scaling) read bounds from the data.

## kMortality: SSA period life table, ages 0-119

Exact transcription of the SSA period life table for 2023, as used in the
2026 Trustees Report (Table 4C6, ssa.gov/oact/STATS/table4c6.html, read
2026-07-24). Six-decimal qx stored as exact `qx*E6` integers; male equals
female from age 109 (source values).

Declared CHOICE: one table year era-wide, so the early era is slightly
under-killed (mortality improved 1990 to 2020). Upgrade: per-year tables
(4C6 covers 2004-2023; older years in the Trustees Report archive) if H3
realism gates need them.

Validation: qx in (0,1); ages strictly increasing; female ≤ male; qx
nondecreasing from age 30. Meaning gates from the lives column: survival
65 to 94 = 8,320/79,084, about 10.5% (< 20%); 22 to 51 = 90,659/98,458,
about 92.1% (> 90%).

## kSources: the source registry (refresh contract)

One row per series: provider, series id, working URL and access method,
fallback URL with axis warnings, value-to-integer transform, earliest
published year, last verification date, status. As of 2026-07-24, FRED
HTML `/data/<SERIES>` views work, `fredgraph.csv` downloads are
policy-blocked, and bls.gov timed out, so FRED mirrors are the working
origins for CPI and the unemployment cross-check. Future H5 adoption
series are PLANNED rows. A refresh reads the registry, never memory, and
updates `lastVerified` in the same round. The registry is re-emitted
verbatim as `econ.provenance`; keep its cells comma-free (semicolons) so
the table stays unquoted and byte-stable.

## Different time periods (coverage extension)

Extending coverage is a data and authority round that rewrites
`era_data.hpp`, never an engine change; the H0.6 lock widens with the data.

- Forward: append 2025 once AWI 2025 publishes (about 2026-10), resolving
  the missing October 2025 CPI and CPS figures from whatever BLS publishes.
  This flips the deliberate tripwire in test_app_options (the default
  2025-01-01 card-fraud start becomes legal) and is model-moving.
- Backward (pre-1990): rows are cheap (`publishedFrom`: CPI 1913, PCE and
  population 1929, unemployment 1948, AWI 1951), but pre-1990 behavioral
  anchors are unresearched.
- Coverage is not calibration: the calibration year stays 2019. Note
  (2026-08): card-fraud prevalence claims once scoped "1991-2020" came
  from a third-party artifact's timeframe; that lineage is removed and the
  level is uncalibrated. The era lock derives from the embedded series
  (1990-2024) and is unaffected.

## Update procedure

1. No silent edits: each value change is an authority round (U-4/U-5
   lineage, now
   [The embedded series](fraud_model_audit.md#m-1-the-embedded-series-1990-2024)
   in `docs/fraud_model_audit.md`), a rewrite of `era_data.hpp`, and of
   this document if precision or meaning changes.
2. Refresh from `kSources`, update its `lastVerified` and status in the
   same rewrite, and keep fields integer-encoded (dollars × 100, index ×
   1000, qx × 1e6, bp).
3. Generation reads the series, so a value refresh is model-moving: a
   named round, one re-pin, green test_econ_catalog and test_econ_tables.
4. A newer SSA table year is a named authority round (record the year).
5. The calibration year changes only with the constants it denominates,
   never in a coverage refresh.
