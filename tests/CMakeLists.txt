# `make test` configures with CMAKE_BUILD_TYPE=Release, whose -DNDEBUG
# silently compiles bare assert() out of every test binary — a gate that
# runs its data flow but enforces nothing. -UNDEBUG comes after the
# config flags on the compile line, so it wins and re-arms assert for
# test translation units only (the job libraries keep their Release
# flags). Corollary: never put side effects inside assert.
set(PL_TEST_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -O2 -UNDEBUG)

# Every test links pl_generator — the complete synthetic-ledger
# generator (the INTERFACE umbrella over the 9 job libraries; L1).
# Extra arguments after the test name are linked as additional
# libraries, e.g. pl_add_test(test_postgres PostgreSQL::PostgreSQL).
function(pl_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE pl_generator ${ARGN})
    target_compile_options(${name} PRIVATE ${PL_TEST_COMPILE_OPTIONS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ---------------------------------------------------------------------
# Unit tests: primitives and taxonomies
# ---------------------------------------------------------------------

pl_add_test(test_pcg64)
pl_add_test(test_seed)
pl_add_test(test_cdf)
pl_add_test(test_validate)
pl_add_test(test_rng)
pl_add_test(test_math)
pl_add_test(test_ids)
pl_add_test(test_channels)
pl_add_test(test_counterparties)
pl_add_test(test_calendar)
pl_add_test(test_app_options)

# ---------------------------------------------------------------------
# Unit tests: chunked pipeline
# ---------------------------------------------------------------------

pl_add_test(test_schedule)
pl_add_test(test_sink)
pl_add_test(test_golden)
pl_add_test(test_fingerprint)

# ---------------------------------------------------------------------
# Unit tests: simulation and exporters (serverless; exporter table sets
# observed via the common::TableCapture test seam — no files)
# ---------------------------------------------------------------------

pl_add_test(test_spending)
pl_add_test(test_minhash_parity)

# geo-causal-v1: the EMBEDDED world geography (no external file/CLI).
# test_geo_catalog pins the compiled-in catalogue (state coverage,
# coherence, distance); test_residence_geography pins population-
# weighted home placement (weighting, zero-residential exclusion,
# per-country, determinism).
pl_add_test(test_geo_catalog)
pl_add_test(test_residence_geography)

# geo-causal-v1 (G2a step-2): MODEL acceptance gates for causal card-present
# merchant selection — distance decay, location-varying decay scale, online
# exclusion, and emergent big-city volume. Serverless + deterministic
# (even-u sampling of the geo pools; no PG, no rng-stream dependency). The
# golden digests pin the bytes; these gates pin the MEANING.
pl_add_test(test_geo_selection)

# macro-history-v1 H0/H1: the EMBEDDED era reference data (constexpr
# tables in synth/econ/era_data.hpp — no external file, no CLI).
# test_econ_catalog pins the typed macro series (contiguous coverage —
# at least 1990-2020, currently through 2024; the 2019 CALIBRATION
# YEAR with its exact scale denominators; anchor ratios, 2009
# deflation, the COVID axis facts, the NBER recession ledger, rising
# population) and the mortality table (shape, interpolation, the two
# cohort survival claims). Since H1 step 2b the series ARE read by
# generation (the nominal scales); since H3 step 1 the mortality table
# is too (the lifespan primitive); since H3 3c-ii the population
# column sizes the join cohort; since H4 the per-capita PCE column
# drives the real activity level.
pl_add_test(test_econ_catalog)

# macro-history-v1 H1 step 2a + H4 step 1: the LEVEL primitives
# (synth/econ/nominal.hpp) — priceScale/wageScale (H1, amounts) and
# pceScale/realPceLevel (H4, the real per-capita consumption path),
# all four level-anchored at the calibration year (exactly 1.0 there)
# with freeze-and-declare clamping outside coverage. This pins the
# levels' MEANING — including the measured dips (1991, 2009, 2020),
# the 2021 rebound, and the deliberately UNPINNED 2001 (that recession
# slowed consumption growth without a per-capita level dip). The
# wiring gates below pin that generation realizes them.
pl_add_test(test_econ_scale)

# macro-history-v1 H1 step 2b + H4 step 2: the WIRING acceptance gates
# (docs/h1_nominal_scale_wiring.md + docs/h4_macro_modulation.md,
# authority U-6 + U-9 and its addendum). Two windowed legs of the same
# 300-person spec at 1991 and 2019.
#   AMOUNT axis (H1): salary means inside an AWI band (~2.48x),
#     spending tickets inside a CPI band (~1.88x), the calibration year
#     realizing verbatim kPricePool subscription prices (scale == 1.0),
#     ATM amounts on the $20 note lattice in BOTH eras, the structuring
#     band intact (class S) while ring-rail fraud scales (sparse
#     DIRECTIONAL band (1.4, 2.6) since the H3 amendment — the fraud
#     budget recomposes with L).
#   COUNT axis (H4): anchor-year session ticket COUNTS ride the real
#     consumption level (1991/2019 within +-15% of ~0.67); the ticket
#     and salary bands above double as the CHANNEL-SEPARATION pin
#     (counts move, amounts must not); the flagged-row count ratio
#     tracks the legit-row count ratio (F = pL/(1-p) rides realized L);
#     and the drift-parity gate is R-AWARE — y/y spend compared in
#     CALIBRATION-LEVEL units (nominal / (priceScale x realPceLevel)),
#     since H4 makes era real growth intentionally unequal.
# Serverless; DIRECTION + magnitude bands, not calibration pins.
pl_add_test(test_econ_wiring)
set_tests_properties(test_econ_wiring PROPERTIES TIMEOUT 600)

# macro-history-v1 H0.5: the era reference tables (schema econ),
# pinned SERVERLESSLY through the TableCapture seam — the rendered
# content of econ.macro_annual / econ.mortality / econ.provenance must
# equal the embedded era data cell for cell. The tables live in their
# OWN schema so the public-schema table golden does not move; THIS
# test is their content pin.
pl_add_test(test_econ_tables)

# macro-history-v1 H2 step 1: the persona-timeline primitive
# (synth/personas/timeline.hpp, authority U-7, contract
# docs/h2_persona_timeline.md) — persona-AT-DATE derived once per
# person on the isolated {"persona-era", personId} lane from the seed
# assignment + birth date. These gates pin the MEANING — the statutory
# FRA schedule, the exact claiming-age mixture, student and
# small-business transition shapes, the personaAt(simStart)==seed
# consistency invariant, monotone irreversibility, and per-person lane
# isolation. The step-2b wiring gates below pin the realization.
pl_add_test(test_persona_timeline)

# macro-history-v1 H2 step 2a: the SINGLE AGE AXIS — the birth-date
# carrier (synth/personas/dob.hpp -> Pack::birthDates) drawn on the
# isolated {"dob", personId} lanes with the retired in-stream draw's
# exact shape. PII renders its Dob FROM the carrier and the SSA
# deposit cohort derives from the REAL birth day-of-month (retiring
# the blake2b syntheticBirthDay). MODEL-MOVING: PII bytes after each
# record's dob position and benefit deposit timestamps moved — the H2
# re-pin absorbs them; these gates pin the carrier's MEANING (bands,
# determinism, lane isolation, statutory cohort mapping).
pl_add_test(test_dob_carrier)

# macro-history-v1 H2 steps 2b+2c AND H3 death wiring: the
# persona-timeline WIRING acceptance gates (docs/h2_persona_timeline.md
# + docs/h3_mortality_estate.md, authority U-7 + addendum). 2b — the
# income switch on an income-only 300-person 4-year GateWorld at 1991.
# 2c — the end-of-window persona resolver, the payday re-anchor
# code-fact, and the retirement spending step (difference-in-
# differences on a base-routines spending leg). H3 — income stops at
# death (salary/SSA/revenue) and the emission loop stops dead
# spenders' person-days (zero post-death spend rows).
# MODEL-MOVING rounds: the four goldens recapture with each.
pl_add_test(test_persona_wiring)
set_tests_properties(test_persona_wiring PROPERTIES TIMEOUT 300)

# macro-history-v1 H3 step 1: the LIFESPAN primitive
# (synth/personas/lifespan.hpp, contract docs/h3_mortality_estate.md)
# — a death date per person on the isolated {"mortality", personId}
# lane, from the single-age carrier and the EMBEDDED SSA 2023 period
# life table (first generation-side consumer of mortality()). These
# gates pin the MEANING: determinism, lane isolation, the
# alive-at-start invariant (hazard conditional on survival to sim
# start), Monte Carlo means matching table-derived expectations, the
# male/female differential, cohort monotonicity, and the age-120 cap.
pl_add_test(test_lifespan)

# macro-history-v1 H3 part 2b: DEATH-CAUSED ESTATES + FUNERALS — the
# uncaused inheritance hazard is retired; every in-window death
# produces one NFDA-anchored funeral (bill channel, decedent's
# account, death+3-10d) and an estate to the heirs 30-90 days later.
# Runs the REAL family pass on a 300-person 4-year world and pins
# causation (every estate/funeral traces to a decedent), the probate
# and funeral timing windows, exactly-one-funeral-per-decedent, and
# existence. MODEL-MOVING: the family lane's rows change wholesale.
pl_add_test(test_estates)

# macro-history-v1 H3 part 3c-ii (CLOSES H3): MEMBERSHIP
# [joinTs, closeTs) — the BEA-sized join cohort on the isolated
# {"join-cohort", personId} lane, the joiner AGE-AXIS repair (ages
# draw at the JOIN date; seed roster byte-stable), ACCOUNT CLOSURE at
# death + settlement ending the contractual line (subscriptions +
# the H1 subscription CPI DEFECT FIX, premiums, obligations, card
# servicing), the claims death stop, the ATM/internal death stops,
# ring fraud never recruiting the dead, the exporters' closed
# lifecycle state, and the standard membership filter's interval
# semantics (authority U-8 + addendum).
# MODEL-MOVING: the four goldens recapture with it.
pl_add_test(test_membership)
set_tests_properties(test_membership PROPERTIES TIMEOUT 300)

pl_add_test(test_pipeline_e2e)
set_tests_properties(test_pipeline_e2e PROPERTIES TIMEOUT 60)

# ---------------------------------------------------------------------
# Integration: PostgreSQL sink (skips with 77 when no server is
# reachable; honors PL_TEST_PG for a custom conninfo)
# ---------------------------------------------------------------------

pl_add_test(test_postgres PostgreSQL::PostgreSQL)
set_tests_properties(test_postgres PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 60)

# ---------------------------------------------------------------------
# Integration: PostgreSQL read-back decode contract. Pins
# encoding::parseKey serverless for all 18 layouts, then — against a
# live server — pins bit-exact round-trip of keys/amount/ts/fraud
# through the streamed transactions table and bit-identical 30/90-day
# accumulation via the SHARED derived::accumulate applied in row_seq
# order. Skips with 77 when no server is reachable; honors PL_TEST_PG.
# ---------------------------------------------------------------------

pl_add_test(test_pg_readback)
set_tests_properties(test_pg_readback PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 60)

# ---------------------------------------------------------------------
# Integration: aml-txn-edges derived bundle read-back parity —
# derived::buildBundle over the retained vector vs readback::buildBundle
# over the streamed transactions table, identical field by field, float
# bits included. Skips with 77; honors PL_TEST_PG.
# ---------------------------------------------------------------------

pl_add_test(test_derived_readback)
set_tests_properties(test_derived_readback PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 300)

# ---------------------------------------------------------------------
# Integration: checkpoint/resume (RunLedger + ResumableSpanSink + the
# resume-mode Postgres sink). Interrupts a run mid-stream, resumes it,
# and requires POSITIONAL byte-identity with an uninterrupted run of
# the same config; also pins tail trim, drifted-config rejection,
# journal-digest and durable-row tamper hard-failures, structural
# fallback to full rewrite, supersede semantics, and journal bounds
# (span rows pruned on complete/fail/supersede). Uses isolated
# pl_resume_* tables. Skips with 77; honors PL_TEST_PG.
# ---------------------------------------------------------------------

pl_add_test(test_resume)
set_tests_properties(test_resume PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# ---------------------------------------------------------------------
# RETIRED (CSV retirement arc, step 5c): the four direct-table parity
# gates — test_mule_ml_direct, test_standard_direct, test_aml_direct,
# test_aml_txn_edges_direct — and their csv_loader verification oracle.
# Parity is structural (the TableMirror COPYs the writer's own rendered
# bytes; there is no second rendering to drift), and the table-digest
# golden below pins the resulting table bytes. Invariants 16–19 retired
# with them.
#
# RETIRED (monolithic-engine retirement): the two binary-level engine
# parity gates — test_windowed_e2e and test_windowed_aml_txn_edges —
# together with the PL_ENGINE override, the app::Engine concept and
# main.cpp's retained-corpus branch. The binary has ONE engine now.
# Engine parity survives at the LIBRARY level, where the retained-
# corpus reference implementation lives on as the executable
# specification: test_arch_equivalence (monolithic run() vs windowed),
# test_production_windowed (runWindowed() reproduces run()), and the
# corpus gates below that use run() as their oracle. Binary output is
# pinned by test_run_golden (serverless stream digest) and
# test_table_golden (live-PG content, both use cases the retired gates
# covered).
# ---------------------------------------------------------------------

# ---------------------------------------------------------------------
# The TABLE-DIGEST GOLDEN (and the FRAUD-VISIBLE PIN). Three sections,
# each with its own baseline (first live run captures a missing one,
# SKIP 77; baselines belong in git IMMEDIATELY, per-server). Tables are
# discovered from the run's own registrations in public.pl_direct_tables
# — the binary runs with no file flags; PostgreSQL content IS the
# output.
#
#   standard    the run golden's exact config; corpus stream in row_seq
#               order + every public-schema direct table.
#               Baseline: tests/golden_tables.md5
#   fraud       aml-txn-edges at the fraud-dense probe config (pop
#               10000, 60d, seed 7, start 1991-01-01 — in-era since the
#               H0.6 card-fraud era lock): pins the fraud-label tables
#               (ShellAccount, SAR + the SAR edges, Alert/CTR/
#               Disposition/InvestigationCase — REQUIRED present; stems
#               verbatim from schema.hpp) and the fraud-dense corpus,
#               closing the fraud-audit-2026-07 coverage finding (the
#               standard config is structurally blind to fraud labels).
#               Baseline: tests/golden_tables_aml.md5
#   card_fraud  card-fraud at the SAME fraud-dense in-era config (rings
#               are probe-verified there; sampleRingCount has no floor,
#               so a small-pop seed could draw zero rings and blind the
#               card view): pins the full cf_* TF_GNN_v3 table set
#               (34 tables; core tables REQUIRED present), hard-requires
#               flag-1 rows in cf_Payment_Transaction (the unauthorized
#               card rail rides card_purchase), and enforces that the
#               corpus stream digest EQUALS the fraud section's (the
#               corpus is use-case-invariant; exporters are export-side
#               only). Baseline: tests/golden_tables_card_fraud.md5
#
# The H0.5 era tables land in their OWN schema (econ), so no section's
# discovery changes; their content pin is the serverless
# test_econ_tables above.
#
# Re-pins must update every touched baseline in the same named commit.
# Skips with 77 when no server is reachable; honors PL_TEST_PG.
# Timeout raised 600 -> 900 with the third binary run (three runs +
# three digest passes now share the budget).
# ---------------------------------------------------------------------

pl_add_test(test_table_golden)
target_compile_definitions(test_table_golden PRIVATE
    PL_BIN_PATH="$<TARGET_FILE:phantomledger>"
    PL_TABLE_BASELINE="${CMAKE_CURRENT_SOURCE_DIR}/golden_tables.md5"
    PL_TABLE_BASELINE_AML="${CMAKE_CURRENT_SOURCE_DIR}/golden_tables_aml.md5"
    PL_TABLE_BASELINE_CARD_FRAUD="${CMAKE_CURRENT_SOURCE_DIR}/golden_tables_card_fraud.md5")
add_dependencies(test_table_golden phantomledger)
set_tests_properties(test_table_golden PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 900)

# ---------------------------------------------------------------------
# End-to-end corpus fingerprint: the binary's printed "Stream digest:
# <hex>  rows: <N>" line vs the pinned baseline (BLAKE2b over the
# rendered corpus stream in row_seq order). The run is serverless
# (PL_FILE_ONLY=1 + PL_PG pinned unreachable) and writes NO files.
# First run on a machine captures a per-toolchain baseline (reported as
# SKIP so capture is explicit); every later run enforces the exact
# line. Delete tests/golden_run.b2sum to re-pin after an intentional
# behavior change or a toolchain upgrade. The baseline file belongs in
# git. Table bytes are pinned by test_table_golden (live PG).
#
# NOTE (macro-history H4): this golden's config starts in 2025, beyond
# the measured era coverage, so it runs at the FROZEN 2024 activity
# level — realPceLevel(2024) = 1.0915, i.e. ~9% ABOVE calibration. The
# H4 re-pin moved it 184,988 -> 197,245 rows for exactly that reason.
# ---------------------------------------------------------------------

pl_add_test(test_run_golden)
target_compile_definitions(test_run_golden PRIVATE
    PL_BIN_PATH="$<TARGET_FILE:phantomledger>"
    PL_GOLDEN_BASELINE="${CMAKE_CURRENT_SOURCE_DIR}/golden_run.b2sum")
add_dependencies(test_run_golden phantomledger)
set_tests_properties(test_run_golden PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 300)

# ---------------------------------------------------------------------
# Fraud amount sampler calibration (registered after test_run_golden so
# the end-to-end golden keeps its historical test number)
# ---------------------------------------------------------------------

pl_add_test(test_fraud_amounts)

# ---------------------------------------------------------------------
# Ownership invariant: every roster person owns at least one account
# ---------------------------------------------------------------------

pl_add_test(test_ownership_invariant)

# ---------------------------------------------------------------------
# S9 acceptance for unauthorized: per-plan keyed streams make a plan's
# output independent of process history and batching
# ---------------------------------------------------------------------

pl_add_test(test_unauthorized_keyed)

# ---------------------------------------------------------------------
# Full-pipeline chunk-strategy invariance: byte-identical corpus at any
# chunk size, with fraud engaged (oracle: the retained-corpus reference
# SimulationPipeline::run())
# ---------------------------------------------------------------------

pl_add_test(test_chunk_invariance)

# ---------------------------------------------------------------------
# Windowed two-phase driver acceptance (migration steps 6-8). All gates
# share the harness in tests/window_leg_support.hpp.
#
#   test_window_invariance  five-leg generation-window matrix (step 6)
#   test_window_bisect      layer-by-layer divergence localization
#   test_thread_invariance  1/4/12 workers at a fixed window (step 7)
#   test_order_ties         replay-order enforcement + tie register
# ---------------------------------------------------------------------

pl_add_test(test_window_invariance)
set_tests_properties(test_window_invariance PROPERTIES TIMEOUT 600)

pl_add_test(test_window_bisect)
set_tests_properties(test_window_bisect PROPERTIES TIMEOUT 600)

pl_add_test(test_thread_invariance)
set_tests_properties(test_thread_invariance PROPERTIES TIMEOUT 600)

pl_add_test(test_order_ties)
set_tests_properties(test_order_ties PROPERTIES TIMEOUT 600)

# ---------------------------------------------------------------------
# ARCHITECTURE EQUIVALENCE — the executable specification. The
# retained-corpus reference implementation (SimulationPipeline::run())
# vs the windowed engine, at the library level; plus its decomposition,
# Session vs Simulator raw spending output. With the app-level
# monolith retired, THIS is the gate that keeps the windowed engine
# honest against the simplest-possible semantics. HARD-ENFORCED.
# (It is also why the H1/H4 era levels needed no separate parity gate:
# both engines read them through the same RateSampler.)
# ---------------------------------------------------------------------

pl_add_test(test_arch_equivalence)
set_tests_properties(test_arch_equivalence PROPERTIES TIMEOUT 600)

pl_add_test(test_session_vs_simulator)
set_tests_properties(test_session_vs_simulator PROPERTIES TIMEOUT 600)

# ---------------------------------------------------------------------
# Bounded binary candidate spool: codec + streaming-cursor contract;
# file-spool leg reproduces the in-memory leg byte-for-byte.
# HARD-ENFORCED.
# ---------------------------------------------------------------------

pl_add_test(test_spool_equivalence)
set_tests_properties(test_spool_equivalence PROPERTIES TIMEOUT 600)

# ---------------------------------------------------------------------
# Production windowed mode: SimulationPipeline::runWindowed must
# reproduce the reference run() byte-for-byte. HARD-ENFORCED.
# ---------------------------------------------------------------------

pl_add_test(test_production_windowed)
set_tests_properties(test_production_windowed PROPERTIES TIMEOUT 600)

# ---------------------------------------------------------------------
# Scale soak (opt-in; SIZE TO MINUTES — soak policy). Skips with 77
# unless PL_SOAK=1. Example:
#
#   PL_SOAK=1 PL_SOAK_POP=10000 ctest --test-dir build \
#       -R test_scale_soak --output-on-failure
# ---------------------------------------------------------------------

pl_add_test(test_scale_soak)
set_tests_properties(test_scale_soak PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 14400)
