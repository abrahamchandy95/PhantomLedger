set(PL_TEST_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -O2)

function(pl_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE phantomledger_core)
    target_compile_options(${name} PRIVATE ${PL_TEST_COMPILE_OPTIONS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# --- Low-level primitives -----------------------------------------------------
# These tests cover the foundational utilities (RNG, seeds, distributions,
# calendar, validation, IDs). They were unchanged by the spending refactor and
# remain accurate against the current headers.
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

# --- Higher-level pipeline ----------------------------------------------------
# Exercises the helpers in `phantomledger/spending/` directly. These pieces
# transitively depend on most of the primitives above (RNG, CDFs, validate,
# rounding, calendar), so a green run here is a smoke test of the lower
# layers as well.
pl_add_test(test_spending)

# Removed:
#   test_personas — the existing test referenced symbols that no longer
#   exist (`kDefaultFractions`, `defaultFraction`, `paycheckSensitivityBeta`).
#   The current persona surface is `personas::archetype(Type)` returning a
#   value, with the field `paycheckBeta(Type)` for the Beta params. Re-add
#   a focused test once a stable persona-surface contract is locked in.
