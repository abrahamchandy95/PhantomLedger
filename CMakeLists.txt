cmake_minimum_required(VERSION 3.23)
project(phantomledger VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(PL_BUILD_TESTS "Build C++ tests" ON)
option(PL_BUILD_PYTHON "Build pybind11 Python module" OFF)
option(PL_ENABLE_NATIVE_OPTIMIZATION "Enable -march=native on supported compilers" OFF)

include(FetchContent)

set(FAKER_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    faker-cxx
    GIT_REPOSITORY https://github.com/cieslarmichal/faker-cxx.git
    GIT_TAG        v4.3.2
)
FetchContent_MakeAvailable(faker-cxx)

set(BUILD_TESTING ON CACHE BOOL "" FORCE)

# PostgreSQL (libpq)
if(APPLE AND NOT DEFINED PostgreSQL_ROOT)
    find_program(PL_HOMEBREW brew)
    if(PL_HOMEBREW)
        foreach(formula libpq postgresql@17 postgresql@16 postgresql@15 postgresql)
            execute_process(
                COMMAND ${PL_HOMEBREW} --prefix ${formula}
                OUTPUT_VARIABLE PL_BREW_PG_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE PL_BREW_PG_RESULT
            )
            if(PL_BREW_PG_RESULT EQUAL 0
               AND EXISTS "${PL_BREW_PG_PREFIX}/include")
                set(PostgreSQL_ROOT "${PL_BREW_PG_PREFIX}")
                message(STATUS "PostgreSQL hint from Homebrew: ${PostgreSQL_ROOT}")
                break()
            endif()
        endforeach()
    endif()
    if(NOT DEFINED PostgreSQL_ROOT
       AND EXISTS "/Applications/Postgres.app/Contents/Versions/latest/include")
        set(PostgreSQL_ROOT
            "/Applications/Postgres.app/Contents/Versions/latest")
        message(STATUS "PostgreSQL hint from Postgres.app: ${PostgreSQL_ROOT}")
    endif()
endif()

find_package(PostgreSQL REQUIRED)

# ---------------------------------------------------------------------
# 1. Core Utilities
# ---------------------------------------------------------------------
set(PL_PRIMITIVES_SOURCES
    src/primitives/time/almanac.cpp
    src/primitives/time/calendar.cpp
    src/primitives/crypto/blake2b.cpp
    src/primitives/postgres/connection.cpp
    src/primitives/random/rng.cpp
)

set(PL_DIAGNOSTICS_SOURCES
    src/diagnostics/logger.cpp
)

# ---------------------------------------------------------------------
# 2. Synthetic Data Generation
# ---------------------------------------------------------------------
set(PL_SYNTH_PII_SOURCES
    src/synth/pii/geonames.cpp
    src/synth/pii/pools.cpp
    src/synth/pii/correlate.cpp
)

set(PL_SYNTH_PRODUCTS_SOURCES
    src/synth/products/sampling/amounts.cpp
    src/synth/products/sampling/dates.cpp
    src/synth/products/installments.cpp
    src/synth/products/obligations.cpp
    src/synth/products/terms/auto_loan.cpp
    src/synth/products/terms/insurance.cpp
    src/synth/products/terms/mortgage.cpp
    src/synth/products/terms/student_loan.cpp
    src/synth/products/terms/tax.cpp
)

set(PL_SYNTH_INFRA_SOURCES
    src/synth/infra/devices.cpp
    src/synth/infra/ips.cpp
    src/synth/infra/rings.cpp
)

# ---------------------------------------------------------------------
# 3. Relationships & Economic Activity
# ---------------------------------------------------------------------
set(PL_RELATIONSHIPS_SOURCES
    src/relationships/family/builder.cpp
    src/relationships/family/links.cpp
    src/relationships/family/partition.cpp
    src/relationships/family/support.cpp
    src/relationships/social/builder.cpp
    src/relationships/social/communities.cpp
    src/relationships/social/sampler.cpp
)

set(PL_ACTIVITY_SPENDING_SOURCES
    src/activity/spending/actors/day.cpp
    src/activity/spending/diagnostics.cpp
    src/activity/spending/dynamics/monthly/evolution.cpp
    src/activity/spending/market/bootstrap.cpp
    src/activity/spending/routing/payments.cpp
    src/activity/spending/simulator/commerce_evolver.cpp
    src/activity/spending/simulator/day_driver.cpp
    src/activity/spending/simulator/day_source.cpp
    src/activity/spending/simulator/driver.cpp
    src/activity/spending/simulator/loop.cpp
    src/activity/spending/simulator/population_dynamics.cpp
    src/activity/spending/simulator/run.cpp
    src/activity/spending/simulator/run_planner.cpp
    src/activity/spending/simulator/session.cpp
    src/activity/spending/simulator/spender_emission_driver.cpp
    src/activity/spending/spenders/prepare.cpp
    src/activity/spending/spenders/targets.cpp
)

# ---------------------------------------------------------------------
# 4. Transactions & Pipeline
# ---------------------------------------------------------------------
set(PL_TRANSACTIONS_SOURCES
    src/transactions/clearing/ledger.cpp
    src/transactions/clearing/loc_accrual.cpp
)

set(PL_PIPELINE_SOURCES
    src/pipeline/simulate.cpp
    src/pipeline/batch/cogen.cpp
    src/pipeline/chunk/schedule.cpp
    src/pipeline/stages/entities.cpp
    src/pipeline/stages/infra.cpp
    src/pipeline/stages/products.cpp
    src/pipeline/stages/transfers/binary_spool.cpp
    src/pipeline/stages/transfers/fraud_emission.cpp
    src/pipeline/stages/transfers/orchestrator.cpp
    src/pipeline/stages/transfers/ledger_replay.cpp
    src/pipeline/stages/transfers/product_replay.cpp
    src/pipeline/stages/transfers/windowed_driver.cpp
    src/pipeline/stages/transfers/windowed_run.cpp
    src/pipeline/stages/transfers/window_sources.cpp
)

# ---------------------------------------------------------------------
# 5. Transfers (Fragmented by Domain)
# ---------------------------------------------------------------------
set(PL_TRANSFERS_CHANNELS_SOURCES
    src/transfers/channels/credit_cards/card_cycle_driver.cpp
    src/transfers/channels/credit_cards/cycle.cpp
    src/transfers/channels/credit_cards/dispute/sampler.cpp
    src/transfers/channels/credit_cards/lifecycle.cpp
    src/transfers/channels/credit_cards/payment.cpp
    src/transfers/channels/credit_cards/session.cpp
    src/transfers/channels/credit_cards/statement.cpp
    src/transfers/channels/insurance/claims.cpp
    src/transfers/channels/insurance/premiums.cpp
    src/transfers/channels/obligations/schedule.cpp
    src/transfers/channels/subscriptions/debits.cpp
)

set(PL_TRANSFERS_FRAUD_SOURCES
    src/transfers/fraud/camouflage.cpp
    src/transfers/fraud/engine.cpp
    src/transfers/fraud/injector.cpp
    src/transfers/fraud/rings.cpp
    src/transfers/fraud/schedule.cpp
    src/transfers/fraud/typologies/bipartite.cpp
    src/transfers/fraud/typologies/classic.cpp
    src/transfers/fraud/typologies/cycle.cpp
    src/transfers/fraud/typologies/dispatch.cpp
    src/transfers/fraud/typologies/funnel.cpp
    src/transfers/fraud/typologies/invoice.cpp
    src/transfers/fraud/typologies/layering.cpp
    src/transfers/fraud/typologies/mule.cpp
    src/transfers/fraud/typologies/scatter_gather.cpp
    src/transfers/fraud/typologies/structuring.cpp
    src/transfers/fraud/typologies/unauthorized.cpp
)

set(PL_TRANSFERS_LEGIT_SOURCES
    src/transfers/legit/assembly.cpp
    src/transfers/legit/blueprints/paydays.cpp
    src/transfers/legit/blueprints/plans.cpp
    src/transfers/legit/ledger/builder.cpp
    src/transfers/legit/ledger/burdens.cpp
    src/transfers/legit/ledger/limits.cpp
    src/transfers/legit/ledger/passes.cpp
    src/transfers/legit/ledger/posting.cpp
    src/transfers/legit/ledger/screenbook.cpp
    src/transfers/legit/ledger/streams.cpp
    src/transfers/legit/routines/atm.cpp
    src/transfers/legit/routines/credit_cards.cpp
    src/transfers/legit/routines/family/allowances.cpp
    src/transfers/legit/routines/family/grandparent_gifts.cpp
    src/transfers/legit/routines/family/inheritance.cpp
    src/transfers/legit/routines/family/parent_gifts.cpp
    src/transfers/legit/routines/family/siblings.cpp
    src/transfers/legit/routines/family/spouse.cpp
    src/transfers/legit/routines/family/support.cpp
    src/transfers/legit/routines/family/tuition.cpp
    src/transfers/legit/routines/internal.cpp
    src/transfers/legit/routines/paychecks.cpp
    src/transfers/legit/routines/relatives.cpp
    src/transfers/legit/routines/spending.cpp
    src/transfers/legit/routines/subscriptions.cpp
    src/transfers/legit/routines/spending_session.cpp
)

# ---------------------------------------------------------------------
# 6. Exporters (Fragmented by Format/Sink)
# ---------------------------------------------------------------------
set(PL_EXPORTER_CORE_SOURCES
    src/exporter/csv.cpp
    src/exporter/labels.cpp
    src/exporter/common/minhash.cpp
    src/exporter/sinks/postgres.cpp
    src/exporter/sinks/run_ledger.cpp
    src/exporter/sinks/table_mirror.cpp
    src/exporter/sinks/txn_readback.cpp
)

set(PL_EXPORTER_STANDARD_SOURCES
    src/exporter/standard/export.cpp
)

set(PL_EXPORTER_MULE_ML_SOURCES
    src/exporter/mule_ml/canonical.cpp
    src/exporter/mule_ml/export.cpp
)

set(PL_EXPORTER_AML_SOURCES
    src/exporter/aml/identity.cpp
    src/exporter/aml/sar.cpp
    src/exporter/aml/vertices.cpp
    src/exporter/aml/edges.cpp
    src/exporter/aml/export.cpp
    src/exporter/aml_txn_edges/derived.cpp
    src/exporter/aml_txn_edges/readback.cpp
    src/exporter/aml_txn_edges/vertices.cpp
    src/exporter/aml_txn_edges/edges.cpp
    src/exporter/aml_txn_edges/export.cpp
)

set(PL_EXPORTER_CARD_FRAUD_SOURCES
    src/exporter/card_fraud/export.cpp
)

# ---------------------------------------------------------------------
# 7. Application Entry
# ---------------------------------------------------------------------
set(PL_APP_SOURCES
    src/app/cli.cpp
    src/app/progress.cpp
    src/app/setup.cpp
)

# ---------------------------------------------------------------------
# Source List Consolidation
#
# PL_ALL_SOURCES stays the AUDIT UNION: the source-list audit below
# compares it against every src/*.cpp on disk, so a translation unit
# that joins the tree without joining a job library is a hard
# configure-time error, exactly as before the L1 split.
# ---------------------------------------------------------------------
set(PL_ALL_SOURCES
    ${PL_PRIMITIVES_SOURCES}
    ${PL_DIAGNOSTICS_SOURCES}
    ${PL_SYNTH_PII_SOURCES}
    ${PL_SYNTH_PRODUCTS_SOURCES}
    ${PL_SYNTH_INFRA_SOURCES}
    ${PL_RELATIONSHIPS_SOURCES}
    ${PL_ACTIVITY_SPENDING_SOURCES}
    ${PL_TRANSACTIONS_SOURCES}
    ${PL_PIPELINE_SOURCES}
    ${PL_TRANSFERS_CHANNELS_SOURCES}
    ${PL_TRANSFERS_FRAUD_SOURCES}
    ${PL_TRANSFERS_LEGIT_SOURCES}
    ${PL_EXPORTER_CORE_SOURCES}
    ${PL_EXPORTER_STANDARD_SOURCES}
    ${PL_EXPORTER_MULE_ML_SOURCES}
    ${PL_EXPORTER_AML_SOURCES}
    ${PL_EXPORTER_CARD_FRAUD_SOURCES}
    ${PL_APP_SOURCES}
)

# ---------------------------------------------------------------------
# Source-list audit
# ---------------------------------------------------------------------

file(GLOB_RECURSE PL_DISCOVERED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

set(PL_DISCOVERED_SOURCES_REL)
foreach(src IN LISTS PL_DISCOVERED_SOURCES)
    file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${src}")
    list(APPEND PL_DISCOVERED_SOURCES_REL "${rel}")
endforeach()

list(REMOVE_ITEM PL_DISCOVERED_SOURCES_REL "src/app/main.cpp")

list(SORT PL_ALL_SOURCES)
list(SORT PL_DISCOVERED_SOURCES_REL)

if(NOT PL_ALL_SOURCES STREQUAL PL_DISCOVERED_SOURCES_REL)
    set(PL_MISSING_FROM_CMAKE)
    foreach(src IN LISTS PL_DISCOVERED_SOURCES_REL)
        if(NOT src IN_LIST PL_ALL_SOURCES)
            list(APPEND PL_MISSING_FROM_CMAKE "  ${src}")
        endif()
    endforeach()

    set(PL_EXTRA_IN_CMAKE)
    foreach(src IN LISTS PL_ALL_SOURCES)
        if(NOT src IN_LIST PL_DISCOVERED_SOURCES_REL)
            list(APPEND PL_EXTRA_IN_CMAKE "  ${src}")
        endif()
    endforeach()

    string(JOIN "\n" PL_MISSING_FROM_CMAKE_MSG ${PL_MISSING_FROM_CMAKE})
    string(JOIN "\n" PL_EXTRA_IN_CMAKE_MSG ${PL_EXTRA_IN_CMAKE})

    message(FATAL_ERROR
        "CMake source list is out of sync with src/*.cpp files.\n"
        "Missing from CMake:\n${PL_MISSING_FROM_CMAKE_MSG}\n"
        "Extra in CMake:\n${PL_EXTRA_IN_CMAKE_MSG}\n"
    )
endif()

# ---------------------------------------------------------------------
# Include-layer lint (L2)
#
# Every #include "phantomledger/..." edge in src/ and include/ is
# checked against the L1 layering DAG at configure time. REPORT MODE
# while PL_LAYER_LINT_FATAL is OFF: violations print as a WARNING
# block and the build continues. See cmake/IncludeLayerLint.cmake for
# the DAG, the findings ledger, and the flip protocol.
# ---------------------------------------------------------------------

include(cmake/IncludeLayerLint.cmake)
pl_run_include_layer_lint(${PL_ALL_SOURCES} src/app/main.cpp)

# ---------------------------------------------------------------------
# Shared build flags (L1)
#
# One INTERFACE target carries the warning/optimization flags every
# PhantomLedger translation unit compiled with before the split, so
# per-TU compile flags stay bit-identical across the 9 libraries.
# Job libraries link it PRIVATE: the flags apply to their own sources
# and never leak to consumers (tests own their flag set).
# ---------------------------------------------------------------------

add_library(pl_build_flags INTERFACE)

if(MSVC)
    target_compile_options(pl_build_flags INTERFACE /W4 /permissive-)
else()
    target_compile_options(pl_build_flags INTERFACE -Wall -Wextra -Wpedantic)

    if(PL_ENABLE_NATIVE_OPTIMIZATION)
        target_compile_options(pl_build_flags INTERFACE -march=native)
    endif()
endif()

# ---------------------------------------------------------------------
# Job libraries (L1)
#
# The monolithic library is split into 9 STATIC libraries, one per
# pipeline job, declared bottom-up in dependency order. NAMING LAW
# (owner directive): no "core"/"base"/"utils" — every target name
# states the job it is responsible for. The declared links form the
# allowed layering DAG (single-responsibility per layer; the include-
# layer lint above enforces it file-by-file, report-mode first).
#
# Both 2026-07-19 reverse edges are CLOSED and the link graph is a
# clean DAG:
#   * pl_activity -> pl_transfers   closed by round C-1 (the simulator
#                                   drives the activity-owned
#                                   CardCycleBilling interface).
#   * pl_transfers -> pl_pipeline   closed by round C-2 (settlement
#                                   chunking policy moved to the
#                                   pipeline's TransferStage; the
#                                   legit builder consumes the
#                                   transfers-owned WorldInputs view,
#                                   adapted at legitWorldInputs()).
# See the findings ledger in cmake/IncludeLayerLint.cmake.
#
# External dependencies are narrowed to the layers that actually use
# them (verified by grep; txn_readback moved export-side in the
# 2026-07-19 untangling round):
#   * libpq      -> pl_primitives PRIVATE (postgres/connection.cpp) and
#                   pl_export PRIVATE (sinks/txn_readback.cpp — the
#                   read-back scan drives libpq cursors directly)
#   * faker-cxx  -> pl_world PRIVATE (only src/synth/pii/pools.cpp
#                   includes faker headers; the previous PUBLIC linkage
#                   on the monolith was over-broad)
# PRIVATE deps of a STATIC library still propagate to the final link
# line, so executables and tests link cleanly without naming either.
# ---------------------------------------------------------------------

function(pl_add_job_library name)
    add_library(${name} STATIC ${ARGN})
    target_include_directories(${name} PUBLIC include)
    target_compile_features(${name} PUBLIC cxx_std_23)
    target_link_libraries(${name} PRIVATE pl_build_flags)
endfunction()

# Deterministic building blocks: time, hashing, RNG, PostgreSQL access.
pl_add_job_library(pl_primitives ${PL_PRIMITIVES_SOURCES})
target_link_libraries(pl_primitives PRIVATE PostgreSQL::PostgreSQL)

# Run observability: the logger (spending-emission diagnostics moved
# INTO pl_activity in the 2026-07-19 untangling round — observability
# of a subsystem lives with the subsystem).
pl_add_job_library(pl_diagnostics ${PL_DIAGNOSTICS_SOURCES})
target_link_libraries(pl_diagnostics PUBLIC pl_primitives)

# The static universe: people/PII, products, devices/IPs/rings,
# family and social graphs.
pl_add_job_library(pl_world
    ${PL_SYNTH_PII_SOURCES}
    ${PL_SYNTH_PRODUCTS_SOURCES}
    ${PL_SYNTH_INFRA_SOURCES}
    ${PL_RELATIONSHIPS_SOURCES}
)
target_link_libraries(pl_world PUBLIC pl_primitives pl_diagnostics)
target_link_libraries(pl_world PRIVATE faker-cxx)

# Balance clearing: the authoritative ledger and LOC accrual.
pl_add_job_library(pl_ledger ${PL_TRANSACTIONS_SOURCES})
target_link_libraries(pl_ledger PUBLIC pl_primitives)

# Day-to-day discretionary spending simulation (incl. its own
# spending-emission diagnostics). Consumes the transaction vocabulary
# (record/draft/factory) and the clearing book (soft screens + emission
# gate) by design — the 2026-07-19 lint scan amendment; see
# cmake/IncludeLayerLint.cmake. Card-cycle billing is consumed through
# the activity-owned CardCycleBilling interface (round C-1), so no
# transfers symbol is referenced here.
pl_add_job_library(pl_activity ${PL_ACTIVITY_SPENDING_SOURCES})
target_link_libraries(pl_activity PUBLIC pl_world pl_ledger)

# Transfer generation: legit flows, channels, fraud typologies.
# Implements the spending simulator's CardCycleBilling seam
# (channels/credit_cards/card_cycle_driver) — that is part of the
# legal pl_transfers -> pl_activity direction.
pl_add_job_library(pl_transfers
    ${PL_TRANSFERS_CHANNELS_SOURCES}
    ${PL_TRANSFERS_FRAUD_SOURCES}
    ${PL_TRANSFERS_LEGIT_SOURCES}
)
target_link_libraries(pl_transfers PUBLIC pl_activity pl_ledger)

# Orchestration: stages, chunking, the windowed streaming engine.
pl_add_job_library(pl_pipeline ${PL_PIPELINE_SOURCES})
target_link_libraries(pl_pipeline PUBLIC pl_transfers)

# Corpus delivery: the four exporters, rendering, PostgreSQL sinks and
# the read-back scan.
pl_add_job_library(pl_export
    ${PL_EXPORTER_CORE_SOURCES}
    ${PL_EXPORTER_STANDARD_SOURCES}
    ${PL_EXPORTER_MULE_ML_SOURCES}
    ${PL_EXPORTER_AML_SOURCES}
    ${PL_EXPORTER_CARD_FRAUD_SOURCES}
)
target_link_libraries(pl_export PUBLIC pl_pipeline)
target_link_libraries(pl_export PRIVATE PostgreSQL::PostgreSQL)

# Binary surface: CLI parsing, progress reporting, run setup.
pl_add_job_library(pl_app ${PL_APP_SOURCES})
target_link_libraries(pl_app PUBLIC pl_export)

# ---------------------------------------------------------------------
# pl_generator — the complete synthetic-ledger generator as ONE
# linkable product (INTERFACE umbrella over the 9 job libraries).
# This is what the binary and every test link; the name states the
# job (PhantomLedger IS a synthetic bank transaction generator) per
# the naming law — the retired name "phantomledger_core" said nothing.
# ---------------------------------------------------------------------

add_library(pl_generator INTERFACE)
target_link_libraries(pl_generator INTERFACE
    pl_app
    pl_export
    pl_pipeline
    pl_transfers
    pl_activity
    pl_ledger
    pl_world
    pl_diagnostics
    pl_primitives
)

# ---------------------------------------------------------------------
# Header IDE target
#
# CMake's compile_commands.json only contains entries for .cpp files.
# When clangd opens a project header (.hpp under include/), it has no
# matching entry and falls back to whatever .cpp seems related — which,
# with faker-cxx in the dependency tree, lands on faker source files
# whose include paths don't cover our include/ tree.
#
# Adding every .hpp to a real CMake target makes CMake emit explicit
# compile_commands entries for them with our project's include path.
# clangd no longer has to guess.
#
# EXCLUDE_FROM_ALL keeps these out of `make` / `make build` so the
# normal build doesn't try to compile headers as translation units.
# The compile_commands entries are still emitted regardless.
# ---------------------------------------------------------------------

file(GLOB_RECURSE PL_PROJECT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

if(PL_PROJECT_HEADERS)
    add_library(phantomledger_headers OBJECT EXCLUDE_FROM_ALL
        ${PL_PROJECT_HEADERS}
    )
    set_target_properties(phantomledger_headers PROPERTIES
        LINKER_LANGUAGE CXX
    )
    # Treat .hpp files as C++ sources so CMake actually compiles them
    # and writes compile_commands.json entries. Without this, CMake
    # ignores .hpp inputs entirely.
    set_source_files_properties(${PL_PROJECT_HEADERS} PROPERTIES
        LANGUAGE CXX
    )
    # Inherit the generator's include paths and compile features so
    # the entries match real build flags exactly. (No project header
    # includes faker or libpq headers — verified — so the narrowed
    # external deps cost this target nothing.)
    target_link_libraries(phantomledger_headers PRIVATE pl_generator)

    # Each header IS the main file of its synthetic translation unit
    # here, so clang's -Wpragma-once-outside-header fires on every
    # project header when this target is built by hand for a
    # whole-tree header typecheck. Silence it for this target only.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(phantomledger_headers PRIVATE
            -Wno-pragma-once-outside-header)
    endif()
endif()

# ---------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------

add_executable(phantomledger src/app/main.cpp)
target_link_libraries(phantomledger PRIVATE pl_generator pl_build_flags)

# ---------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------

if(PL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS phantomledger
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS
    pl_primitives
    pl_diagnostics
    pl_world
    pl_activity
    pl_ledger
    pl_transfers
    pl_pipeline
    pl_export
    pl_app
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)
