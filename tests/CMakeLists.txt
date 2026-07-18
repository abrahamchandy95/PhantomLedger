# `make test` configures with CMAKE_BUILD_TYPE=Release, whose -DNDEBUG
# silently compiles bare assert() out of every test binary — a gate that
# runs its data flow but enforces nothing. -UNDEBUG comes after the
# config flags on the compile line, so it wins and re-arms assert for
# test translation units only (the core library keeps its Release
# flags). Corollary: never put side effects inside assert.
set(PL_TEST_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -O2 -UNDEBUG)

# Extra arguments after the test name are linked as additional
# libraries, e.g. pl_add_test(test_postgres PostgreSQL::PostgreSQL).
function(pl_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE phantomledger_core ${ARGN})
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
# ---------------------------------------------------------------------

# ---------------------------------------------------------------------
# The TABLE-DIGEST GOLDEN (and the FRAUD-VISIBLE PIN). Two sections,
# each with its own baseline (first live run captures a missing one,
# SKIP 77; baselines belong in git IMMEDIATELY, per-server). Tables are
# discovered from the run's own registrations in public.pl_direct_tables
# — the binary runs with no file flags; PostgreSQL content IS the
# output.
#
#   standard  the run golden's exact config; corpus stream in row_seq
#             order + every public-schema direct table.
#             Baseline: tests/golden_tables.md5
#   fraud     aml-txn-edges at the fraud-dense probe config (pop 10000,
#             60d, seed 7): pins the fraud-label tables (ShellAccount,
#             SAR + the SAR edges, Alert/CTR/Disposition/
#             InvestigationCase — REQUIRED present; stems verbatim from
#             schema.hpp) and the fraud-dense corpus, closing the
#             fraud-audit-2026-07 coverage finding (the standard config
#             is structurally blind to fraud labels).
#             Baseline: tests/golden_tables_aml.md5
#
# Re-pins must update every touched baseline in the same named commit.
# Skips with 77 when no server is reachable; honors PL_TEST_PG.
# ---------------------------------------------------------------------

pl_add_test(test_table_golden)
target_compile_definitions(test_table_golden PRIVATE
    PL_BIN_PATH="$<TARGET_FILE:phantomledger>"
    PL_TABLE_BASELINE="${CMAKE_CURRENT_SOURCE_DIR}/golden_tables.md5"
    PL_TABLE_BASELINE_AML="${CMAKE_CURRENT_SOURCE_DIR}/golden_tables_aml.md5")
add_dependencies(test_table_golden phantomledger)
set_tests_properties(test_table_golden PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 600)

# ---------------------------------------------------------------------
# Integration: aml-txn-edges ENGINE parity (binary runs against a LIVE
# server — the windowed leg's derived analytics read the streamed
# corpus back from PostgreSQL). Reference engine (PL_ENGINE=monolithic,
# test-only env override) vs the DEFAULT selection — which this gate
# thereby pins as auto -> windowed for aml-txn-edges: same stream
# digest, digest-identical PostgreSQL content (every registered
# aml_txn_edges table + the corpus in row_seq order, per leg). Skips
# with 77; honors PL_TEST_PG.
# ---------------------------------------------------------------------

pl_add_test(test_windowed_aml_txn_edges)
target_compile_definitions(test_windowed_aml_txn_edges PRIVATE
    PL_BIN_PATH="$<TARGET_FILE:phantomledger>")
add_dependencies(test_windowed_aml_txn_edges phantomledger)
set_tests_properties(test_windowed_aml_txn_edges PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 600)

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
# chunk size, with fraud engaged
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
# Monolithic vs windowed architecture equivalence (step 12), and its
# decomposition: Session vs Simulator raw spending output.
# HARD-ENFORCED.
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
# reproduce run() byte-for-byte. HARD-ENFORCED.
# ---------------------------------------------------------------------

pl_add_test(test_production_windowed)
set_tests_properties(test_production_windowed PROPERTIES TIMEOUT 600)

# ---------------------------------------------------------------------
# Windowed CLI end-to-end: the DEFAULT engine (auto -> windowed) vs the
# monolithic reference engine (PL_ENGINE=monolithic): same printed
# stream digest per use case, default pinned via the Engine: log line
# (serverless: PL_FILE_ONLY=1 + PL_PG pinned unreachable; no files
# written). HARD-ENFORCED.
# ---------------------------------------------------------------------

pl_add_test(test_windowed_e2e)
target_compile_definitions(test_windowed_e2e PRIVATE
    PL_BIN_PATH="$<TARGET_FILE:phantomledger>")
add_dependencies(test_windowed_e2e phantomledger)
set_tests_properties(test_windowed_e2e PROPERTIES TIMEOUT 600)

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
