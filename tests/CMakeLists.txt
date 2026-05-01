set(PL_TEST_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -O2)

function(pl_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE phantomledger_core)
    target_compile_options(${name} PRIVATE ${PL_TEST_COMPILE_OPTIONS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()


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


pl_add_test(test_spending)


pl_add_test(test_minhash_parity)

pl_add_test(test_pipeline_e2e)

set_tests_properties(test_pipeline_e2e PROPERTIES TIMEOUT 60)


