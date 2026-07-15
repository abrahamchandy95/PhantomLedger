set(PL_TEST_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -O2)

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

# ---------------------------------------------------------------------
# Unit tests: simulation and exporters
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
# End-to-end output fingerprint. First run on a machine captures a
# per-toolchain baseline (reported as SKIP so capture is explicit);
# every later run enforces byte-identical output. Delete
# tests/golden_run.b2sum to re-pin after an intentional behavior
# change or a toolchain upgrade. The baseline file belongs in git.
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
# (structural guarantee from synth/accounts/counts.hpp, enforced as a
# gate so no synthesis refactor can silently drop it)
# ---------------------------------------------------------------------

pl_add_test(test_ownership_invariant)

# ---------------------------------------------------------------------
# S9 acceptance for unauthorized: per-plan keyed streams make a plan's
# output independent of process history and batching
# ---------------------------------------------------------------------

pl_add_test(test_unauthorized_keyed)
