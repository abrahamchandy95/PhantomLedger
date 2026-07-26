# Era reference data — provenance & refresh contract (macro-history-v1)

The 1990–2024 US economic era (price level, wages, spending,
unemployment, recessions, population), the mortality table, and the H1
CALIBRATION YEAR are **EMBEDDED as constexpr tables** in
`include/phantomledger/synth/econ/era_data.hpp` — the pinned artifact
that replaced the retired `data/econ/*.csv` files per the owner's
minimize-repo-data-files directive (2026-07-24). This document is the
provenance and refresh contract for that header.

**CONTRACT:** the data is validated + typed by
`synth::econ::{macroSeries(),mortality()}` (src/synth/econ/catalog.cpp),
meaning-gated by `tests/test_econ_catalog.cpp`, and rendered into
PostgreSQL for every use case as `econ.macro_annual` /
`econ.mortality` / `econ.provenance` (exporter::econ, pinned by
`tests/test_econ_tables.cpp`) — but **UNREAD BY GENERATION** until the
named macro-history H1+ model rounds. The one sanctioned
non-generation reader is the app-layer H0.6 card-fraud era lock
(coverage bounds only).

## The calibration year (H1 anchor) — owner-approved CHOICE

`kCalibrationYear = 2019` is **the year the authority's calibrated
dollar constants are denominated in**. The conformance program
anchored PhantomLedger's dollar magnitudes to measurements taken
roughly 2015–2024; the owner-approved CHOICE
(macro-history-2026-07c) declares that calibrated economy
2019-denominated — the last full simulated year of the canonical
card-fraud window and the last pre-COVID year, so the era ramps to a
scale of exactly 1.0 at its endpoint. H1 consumers scale by
`index(year) / index(kCalibrationYear)` (AWI for incomes, CPI for
prices).

**Design rule (the owner's 2050 criterion):** the calibration year is
a PROVENANCE FACT OF THE CALIBRATION DATA — not "the present", not the
last covered year, never the wall clock or the run's start date. An
operator in 2050 studying this frozen era reads "constants in 2019
dollars" exactly as meaningfully as an operator today. It therefore
lives IN the pinned data (single artifact, builder-validated inside
coverage) and changes ONLY together with the constants it denominates,
in a named model-moving round. Rejected alternatives, recorded so they
stay rejected: latest-coverage-year (a data refresh would silently
re-denominate the whole economy — coverage ≠ calibration), wall-clock
or run-start anchoring (breaks determinism/comparability).
Per-constant measurement vintages are the registered upgrade path.

## kMacroAnnual — one row per calendar year, 1990–2024 contiguous

All columns integer-encoded; the loader/builder never parses floats.

| Field | Meaning | Encoding | Source series | Status |
|---|---|---|---|---|
| `year` | calendar year | int | — | — |
| `cpiUE3` | CPI-U ANNUAL AVERAGE, all items, US city average (1982-84=100) | index × 1000 | BLS CUUR0000SA0 == FRED `CPIAUCNS` (monthly NSA) | **VERIFIED EXACT** 2026-07-24 — the official annual average IS the mean of the 12 NSA monthly indexes; recomputed from the FRED data view for every year and matched to the third decimal. The H1 round corrected 1990–2006 from earlier tenths-rounded transcriptions to the exact values |
| `awiCents` | SSA national Average Wage Index | dollars × 100 | ssa.gov/oact/cola/awiseries.html | **VERIFIED EXACT** — all values matched the live page, 2026-07-24. **LAG: the AWI for year N publishes ~October N+1 — THE binding coverage constraint** |
| `pceDollars` | nominal per-capita personal consumption expenditures | exact dollars | BEA (GDP release) via FRED `A794RC0A052NBEA` | **VERIFIED EXACT** — FRED HTML data view, vintage 2026-04-09 |
| `unempBp` | U-3 unemployment, ANNUAL AVERAGE | percent × 100 | BLS series LNS14000000 | official annual averages, **cross-checked within 0.1pp** 2026-07-24 against FRED `UNRATENSA` monthly means (the official statistic is a ratio of annual averages, not a mean of monthly rates — the two differ in the last digit for a few years, e.g. 2011, 2021) |
| `recessionMonths` | NBER recession months in that year | int 0–12 | NBER business-cycle dating | dates [Certain]; convention below |
| `populationThousands` | US population, BEA NIPA MIDPERIOD | thousands, exact | BEA via FRED `B230RC0A052NBEA` | **VERIFIED EXACT** — vintage 2026-02-20. AXIS: the per-capita-PCE denominator, NOT the Census July-1 resident estimate (<0.3% apart; never mix) |

**AXIS NOTES (check the axis before the number):**

* Unemployment is the ANNUAL AVERAGE, not the monthly peak. Era
  monthly peaks are HIGHER: 7.8% (1992-06), 6.3% (2003-06), 10.0%
  (2009-10), 14.7% (2020-04). H4 recession modeling needs a monthly
  series added in its own round.
* `recessionMonths` counts months STRICTLY AFTER the NBER peak month
  through the trough month, so per-year values sum to the published
  durations: 1990:5+1991:3=8; 2001:8; 2008:12+2009:6=18; 2020:2.
  2021–2024 carry zero.
* The canonical run window `[1991-01-01, 2020-01-01)` ends BEFORE the
  COVID recession; the 2020–2024 rows are DATA (the H0.6 lock accepts
  windows into them), but COVID-era BEHAVIOR (EIP checks, the 2020
  saving spike) stays a P2 module — the same status every pre-2020
  recession has until H4 wires macro modulation.
* Anchor facts (pinned by test_econ_catalog): CPI 2019/1991 ≈ 1.88;
  AWI ≈ 2.48; per-capita PCE 2019/1990 = 43,682/15,225 ≈ 2.87; 2009 is
  the era's only annual CPI deflation (and the AWI's only dip); the
  2020 COVID rows show the era's only per-capita PCE dip and an 8.1%
  annual-average unemployment; 2021→2022 is the era's largest annual
  CPI jump (~8.0%); population strictly increases every year
  (slowest: 2021).

## Why coverage ends at 2024 (as of 2026-07)

A row exists only when EVERY column is fully MEASURED — no
projections, no partial years, no mixing of measured and estimated
cells. As of 2026-07 no later year qualifies, for three independent
reasons:

1. **AWI 2025 publishes ~October 2026** (the structural ~Oct N+1 lag).
2. **The October 2025 CPI release was cancelled outright** (federal
   government shutdown) — FRED shows the observation as missing, so no
   official 2025 CPI annual average exists.
3. **The October 2025 CPS household survey was never collected** (same
   shutdown) — no official 2025 unemployment annual average either.

The measured frontier ALWAYS lags now-time; that is a permanent
structural fact of measured history that any operator — today or in
2050 — inherits. The engine never hardcodes the frontier: the builder
requires only first ≤ 1990 and last ≥ 2020, and every consumer (the
H0.6 lock, H1 scaling) reads bounds from the data.

## kMortality — SSA period life table, single ages 0–119

**VERIFIED EXACT: a full transcription of the SSA PERIOD LIFE TABLE
FOR 2023, as used in the 2026 Trustees Report (Actuarial Life Table
4C6, ssa.gov/oact/STATS/table4c6.html, read 2026-07-24).** Six-decimal
source qx = exact `qx*E6` integers; male == female from age 109 up
(source values).

**DECLARED SIMPLIFICATION (CHOICE): one period-table year era-wide.**
Real mortality improved 1990→2020, so the early era is slightly
under-killed. Upgrade path: per-year SSA period tables (the 4C6 page's
dropdown covers 2004–2023; older years in the Trustees Report
archive — see the registry) if H3 realism gates demand it.

Validation (builder + test): qx ∈ (0,1); ages strictly increasing;
female ≤ male at every age; qx nondecreasing from age 30 up. Meaning
gates cross-checked against the source lives column: survival 65→94 =
8,320/79,084 ≈ 10.5% (< 20%) and 22→51 = 90,659/98,458 ≈ 92.1%
(> 90%) — the two cohort claims of the macro-history arc.

## kSources — the source registry (refresh contract)

One row per series: provider, series id, the exact WORKING URL and
access method (FRED HTML `/data/<SERIES>` views work; `fredgraph.csv`
is a file download and policy-blocked; bls.gov timed out 2026-07-24 —
the FRED mirrors are the verified working origins for CPI and the
unemployment cross-check), a fallback URL with axis warnings, the
value→integer transform, how far back the source publishes, the last
live-verification date, and status. H5's future adoption series are
pre-registered as PLANNED rows. A refresh round reads THE REGISTRY,
never memory, and updates `lastVerified` in the same round.

The registry is re-emitted verbatim as the `econ.provenance` direct
table, so provenance travels with the data into every database. Keep
registry cell text comma-free (semicolons) so the rendered table stays
unquoted and byte-stable.

## Different time periods (coverage extension)

Coverage extension is a DATA + AUTHORITY round — a rewrite of
`era_data.hpp`, never an engine change (the builder requires first ≤
1990 and last ≥ 2020; the H0.6 card-fraud lock reads coverage bounds
from the data, so extending it widens the runnable window
automatically).

* **Forward (append 2025 when AWI 2025 publishes, ~2026-10):** also
  resolve the missing Oct-2025 CPI/CPS observations per whatever BLS
  publishes as the official annual figures. The refresh consciously
  flips the DELIBERATE TRIPWIRE in test_app_options (the default
  2025-01-01 card-fraud start becomes legal) and, once H1 wiring
  exists, is MODEL-MOVING for scaled use cases. CAVEAT — series ≠
  behavior: windows crossing 2020-04 get correct nominal LEVELS from
  these rows, but COVID/EIP BEHAVIOR stays P2 until H4-time.
* **Backward (pre-1990):** rows are cheap (registry `publishedFrom`:
  CPI 1913, PCE/population 1929, unemployment 1948, AWI 1951) but
  pre-1990 behavioral era anchors are an unresearched arc of their own.
* **Coverage ≠ calibration:** the TabFormer-calibration claims stay
  1991–2020, and the calibration year stays 2019, no matter how far
  coverage extends.

## Update procedure

1. Never edit values silently: every value change is an authority
   round (docs/fraud_model_audit.md U-4/U-5 lineage via a one-shot
   merge script) + a rewrite of `era_data.hpp` + this document if
   precision/meaning changes.
2. Refresh reads `kSources` (URLs, access method, transform), updates
   its `lastVerified`/status fields in the same rewrite, and keeps
   every field integer-encoded (dollars×100, index×1000, qx×1e6, bp).
3. While the series stay UNREAD by generation, a data refresh moves
   ZERO goldens (test_econ_catalog + test_econ_tables re-run green).
   Once H1+ consumers exist, a value refresh is MODEL-MOVING: named
   round + one re-pin.
4. A newer SSA table year is a new named authority round (record the
   table year).
5. The calibration year changes ONLY with the constants it denominates
   (a re-calibration round), never as part of a coverage refresh.
