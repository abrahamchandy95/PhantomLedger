cmake_minimum_required(VERSION 3.23)
project(phantomledger VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(PL_BUILD_TESTS "Build C++ tests" ON)
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
# Module source lists (L3 physical layout)
#
# One directory per layer under libs/: each module is self-contained as
# libs/<layer>/{include/phantomledger/<layer>/..., src/...}, so a
# review of libs/<layer>/ sees exactly one layer and nothing else.
# Header include paths are unchanged (every header keeps its
# phantomledger/<layer>/ prefix inside its module's include/).
# ---------------------------------------------------------------------

# ---------------------------------------------------------------------
# 1. Core Utilities
# ---------------------------------------------------------------------
set(PL_PRIMITIVES_SOURCES
    libs/primitives/src/time/almanac.cpp
    libs/primitives/src/time/calendar.cpp
    libs/primitives/src/crypto/blake2b.cpp
    libs/primitives/src/postgres/connection.cpp
    libs/primitives/src/random/rng.cpp
)

set(PL_DIAGNOSTICS_SOURCES
    libs/diagnostics/src/logger.cpp
)

# ---------------------------------------------------------------------
# 2. Synthetic Data Generation
# ---------------------------------------------------------------------
set(PL_SYNTH_PII_SOURCES
    libs/synth/src/pii/geonames.cpp
    libs/synth/src/pii/pools.cpp
    libs/synth/src/pii/correlate.cpp
)

set(PL_SYNTH_PRODUCTS_SOURCES
    libs/synth/src/products/sampling/amounts.cpp
    libs/synth/src/products/sampling/dates.cpp
    libs/synth/src/products/installments.cpp
    libs/synth/src/products/obligations.cpp
    libs/synth/src/products/terms/auto_loan.cpp
    libs/synth/src/products/terms/insurance.cpp
    libs/synth/src/products/terms/mortgage.cpp
    libs/synth/src/products/terms/student_loan.cpp
    libs/synth/src/products/terms/tax.cpp
)

set(PL_SYNTH_INFRA_SOURCES
    libs/synth/src/infra/devices.cpp
    libs/synth/src/infra/ips.cpp
    libs/synth/src/infra/rings.cpp
)

# ---------------------------------------------------------------------
# 3. Relationships & Economic Activity
# ---------------------------------------------------------------------
set(PL_RELATIONSHIPS_SOURCES
    libs/relationships/src/family/builder.cpp
    libs/relationships/src/family/links.cpp
    libs/relationships/src/family/partition.cpp
    libs/relationships/src/family/support.cpp
    libs/relationships/src/social/builder.cpp
    libs/relationships/src/social/communities.cpp
    libs/relationships/src/social/sampler.cpp
)

set(PL_ACTIVITY_SPENDING_SOURCES
    libs/activity/src/spending/actors/day.cpp
    libs/activity/src/spending/diagnostics.cpp
    libs/activity/src/spending/dynamics/monthly/evolution.cpp
    libs/activity/src/spending/market/bootstrap.cpp
    libs/activity/src/spending/routing/payments.cpp
    libs/activity/src/spending/simulator/commerce_evolver.cpp
    libs/activity/src/spending/simulator/day_driver.cpp
    libs/activity/src/spending/simulator/day_source.cpp
    libs/activity/src/spending/simulator/driver.cpp
    libs/activity/src/spending/simulator/loop.cpp
    libs/activity/src/spending/simulator/population_dynamics.cpp
    libs/activity/src/spending/simulator/run.cpp
    libs/activity/src/spending/simulator/run_planner.cpp
    libs/activity/src/spending/simulator/session.cpp
    libs/activity/src/spending/simulator/spender_emission_driver.cpp
    libs/activity/src/spending/spenders/prepare.cpp
    libs/activity/src/spending/spenders/targets.cpp
)

# ---------------------------------------------------------------------
# 4. Transactions & Pipeline
# ---------------------------------------------------------------------
set(PL_TRANSACTIONS_SOURCES
    libs/transactions/src/clearing/ledger.cpp
    libs/transactions/src/clearing/loc_accrual.cpp
)

set(PL_PIPELINE_SOURCES
    libs/pipeline/src/simulate.cpp
    libs/pipeline/src/batch/cogen.cpp
    libs/pipeline/src/chunk/schedule.cpp
    libs/pipeline/src/stages/entities.cpp
    libs/pipeline/src/stages/infra.cpp
    libs/pipeline/src/stages/products.cpp
    libs/pipeline/src/stages/transfers/binary_spool.cpp
    libs/pipeline/src/stages/transfers/fraud_emission.cpp
    libs/pipeline/src/stages/transfers/orchestrator.cpp
    libs/pipeline/src/stages/transfers/ledger_replay.cpp
    libs/pipeline/src/stages/transfers/product_replay.cpp
    libs/pipeline/src/stages/transfers/windowed_driver.cpp
    libs/pipeline/src/stages/transfers/windowed_run.cpp
    libs/pipeline/src/stages/transfers/window_sources.cpp
)

# ---------------------------------------------------------------------
# 5. Transfers (Fragmented by Domain)
# ---------------------------------------------------------------------
set(PL_TRANSFERS_CHANNELS_SOURCES
    libs/transfers/src/channels/credit_cards/card_cycle_driver.cpp
    libs/transfers/src/channels/credit_cards/cycle.cpp
    libs/transfers/src/channels/credit_cards/dispute/sampler.cpp
    libs/transfers/src/channels/credit_cards/lifecycle.cpp
    libs/transfers/src/channels/credit_cards/payment.cpp
    libs/transfers/src/channels/credit_cards/session.cpp
    libs/transfers/src/channels/credit_cards/statement.cpp
    libs/transfers/src/channels/insurance/claims.cpp
    libs/transfers/src/channels/insurance/premiums.cpp
    libs/transfers/src/channels/obligations/schedule.cpp
    libs/transfers/src/channels/subscriptions/debits.cpp
)

set(PL_TRANSFERS_FRAUD_SOURCES
    libs/transfers/src/fraud/camouflage.cpp
    libs/transfers/src/fraud/engine.cpp
    libs/transfers/src/fraud/injector.cpp
    libs/transfers/src/fraud/rings.cpp
    libs/transfers/src/fraud/schedule.cpp
    libs/transfers/src/fraud/typologies/bipartite.cpp
    libs/transfers/src/fraud/typologies/classic.cpp
    libs/transfers/src/fraud/typologies/cycle.cpp
    libs/transfers/src/fraud/typologies/dispatch.cpp
    libs/transfers/src/fraud/typologies/funnel.cpp
    libs/transfers/src/fraud/typologies/invoice.cpp
    libs/transfers/src/fraud/typologies/layering.cpp
    libs/transfers/src/fraud/typologies/mule.cpp
    libs/transfers/src/fraud/typologies/scatter_gather.cpp
    libs/transfers/src/fraud/typologies/structuring.cpp
    libs/transfers/src/fraud/typologies/unauthorized.cpp
)

set(PL_TRANSFERS_LEGIT_SOURCES
    libs/transfers/src/legit/assembly.cpp
    libs/transfers/src/legit/blueprints/paydays.cpp
    libs/transfers/src/legit/blueprints/plans.cpp
    libs/transfers/src/legit/ledger/builder.cpp
    libs/transfers/src/legit/ledger/burdens.cpp
    libs/transfers/src/legit/ledger/limits.cpp
    libs/transfers/src/legit/ledger/passes.cpp
    libs/transfers/src/legit/ledger/posting.cpp
    libs/transfers/src/legit/ledger/screenbook.cpp
    libs/transfers/src/legit/ledger/streams.cpp
    libs/transfers/src/legit/routines/atm.cpp
    libs/transfers/src/legit/routines/credit_cards.cpp
    libs/transfers/src/legit/routines/family/allowances.cpp
    libs/transfers/src/legit/routines/family/grandparent_gifts.cpp
    libs/transfers/src/legit/routines/family/inheritance.cpp
    libs/transfers/src/legit/routines/family/parent_gifts.cpp
    libs/transfers/src/legit/routines/family/siblings.cpp
    libs/transfers/src/legit/routines/family/spouse.cpp
    libs/transfers/src/legit/routines/family/support.cpp
    libs/transfers/src/legit/routines/family/tuition.cpp
    libs/transfers/src/legit/routines/internal.cpp
    libs/transfers/src/legit/routines/paychecks.cpp
    libs/transfers/src/legit/routines/relatives.cpp
    libs/transfers/src/legit/routines/spending.cpp
    libs/transfers/src/legit/routines/subscriptions.cpp
    libs/transfers/src/legit/routines/spending_session.cpp
)

# ---------------------------------------------------------------------
# 6. Exporters (Fragmented by Format/Sink)
# ---------------------------------------------------------------------
set(PL_EXPORTER_CORE_SOURCES
    libs/exporter/src/csv.cpp
    libs/exporter/src/labels.cpp
    libs/exporter/src/common/minhash.cpp
    libs/exporter/src/sinks/postgres.cpp
    libs/exporter/src/sinks/run_ledger.cpp
    libs/exporter/src/sinks/table_mirror.cpp
    libs/exporter/src/sinks/txn_readback.cpp
)

set(PL_EXPORTER_STANDARD_SOURCES
    libs/exporter/src/standard/export.cpp
)

set(PL_EXPORTER_MULE_ML_SOURCES
    libs/exporter/src/mule_ml/canonical.cpp
    libs/exporter/src/mule_ml/export.cpp
)

set(PL_EXPORTER_AML_SOURCES
    libs/exporter/src/aml/identity.cpp
    libs/exporter/src/aml/sar.cpp
    libs/exporter/src/aml/vertices.cpp
    libs/exporter/src/aml/edges.cpp
    libs/exporter/src/aml/export.cpp
    libs/exporter/src/aml_txn_edges/derived.cpp
    libs/exporter/src/aml_txn_edges/readback.cpp
    libs/exporter/src/aml_txn_edges/vertices.cpp
    libs/exporter/src/aml_txn_edges/edges.cpp
    libs/exporter/src/aml_txn_edges/export.cpp
)

set(PL_EXPORTER_CARD_FRAUD_SOURCES
    libs/exporter/src/card_fraud/export.cpp
)

# ---------------------------------------------------------------------
# 7. Application Entry
# ---------------------------------------------------------------------
set(PL_APP_SOURCES
    libs/app/src/cli.cpp
    libs/app/src/progress.cpp
    libs/app/src/setup.cpp
)

# ---------------------------------------------------------------------
# Source List Consolidation
#
# PL_ALL_SOURCES stays the AUDIT UNION: the source-list audit below
# compares it against every libs/*/src .cpp on disk, so a translation
# unit that joins the tree without joining a job library is a hard
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
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/*.cpp"
)

set(PL_DISCOVERED_SOURCES_REL)
foreach(src IN LISTS PL_DISCOVERED_SOURCES)
    file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${src}")
    list(APPEND PL_DISCOVERED_SOURCES_REL "${rel}")
endforeach()

list(REMOVE_ITEM PL_DISCOVERED_SOURCES_REL "libs/app/src/main.cpp")

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
        "CMake source list is out of sync with libs/*/src .cpp files.\n"
        "Missing from CMake:\n${PL_MISSING_FROM_CMAKE_MSG}\n"
        "Extra in CMake:\n${PL_EXTRA_IN_CMAKE_MSG}\n"
    )
endif()

# ---------------------------------------------------------------------
# Include-layer lint (L2)
#
# Every #include "phantomledger/..." edge in libs/ is checked against
# the L1 layering DAG at configure time. FATAL BY DEFAULT since the L2
# close-out (2026-07-19): an illegal edge fails the configure. See
# cmake/IncludeLayerLint.cmake for the DAG, the findings ledger, and
# the escape hatch.
# ---------------------------------------------------------------------

include(cmake/IncludeLayerLint.cmake)
pl_run_include_layer_lint(${PL_ALL_SOURCES} libs/app/src/main.cpp)

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
# Vocabulary modules (L3)
#
# The four header-only vocabulary trees are real INTERFACE targets:
# what a layer can see is now decided by the include paths its links
# carry, so the vocabulary DAG holds in CMake itself, not only in the
# lint. pl_vocabulary is the complete shared language for the layers
# allowed to speak all of it (every job layer except pl_primitives,
# which sees taxonomies only).
# ---------------------------------------------------------------------

add_library(pl_taxonomies INTERFACE)
target_include_directories(pl_taxonomies INTERFACE libs/taxonomies/include)

add_library(pl_entities INTERFACE)
target_include_directories(pl_entities INTERFACE libs/entities/include)
target_link_libraries(pl_entities INTERFACE pl_primitives pl_taxonomies)

add_library(pl_encoding INTERFACE)
target_include_directories(pl_encoding INTERFACE libs/encoding/include)
target_link_libraries(pl_encoding INTERFACE pl_entities)

add_library(pl_math INTERFACE)
target_include_directories(pl_math INTERFACE libs/math/include)
target_link_libraries(pl_math INTERFACE pl_entities)

add_library(pl_vocabulary INTERFACE)
target_link_libraries(pl_vocabulary INTERFACE
    pl_taxonomies pl_entities pl_encoding pl_math)

# ---------------------------------------------------------------------
# Job libraries (L1 split, L3 physical layout)
#
# The monolithic library is split into 9 STATIC libraries, one per
# pipeline job, declared bottom-up in dependency order; each carries
# ONLY its own module include dir(s) PUBLIC, so header visibility
# follows the link graph. NAMING LAW (owner directive): no "core"/
# "base"/"utils" — every target name states the job it is responsible
# for. The declared links form the allowed layering DAG (single-
# responsibility per layer; the include-layer lint above enforces it
# file-by-file, fatal by default).
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
#   * faker-cxx  -> pl_world PRIVATE (only libs/synth/src/pii/pools.cpp
#                   includes faker headers; the previous PUBLIC linkage
#                   on the monolith was over-broad)
# PRIVATE deps of a STATIC library still propagate to the final link
# line, so executables and tests link cleanly without naming either.
# ---------------------------------------------------------------------

function(pl_add_job_library name)
    add_library(${name} STATIC ${ARGN})
    target_compile_features(${name} PUBLIC cxx_std_23)
    target_link_libraries(${name} PRIVATE pl_build_flags)
endfunction()

# Deterministic building blocks: time, hashing, RNG, PostgreSQL access.
pl_add_job_library(pl_primitives ${PL_PRIMITIVES_SOURCES})
target_include_directories(pl_primitives PUBLIC libs/primitives/include)
target_link_libraries(pl_primitives PUBLIC pl_taxonomies)
target_link_libraries(pl_primitives PRIVATE PostgreSQL::PostgreSQL)

# Run observability: the logger (spending-emission diagnostics moved
# INTO pl_activity in the 2026-07-19 untangling round — observability
# of a subsystem lives with the subsystem). SILENT BY DEFAULT (runtime
# level warn); the Makefile's run-info/run-debug/run-trace/run-mem
# targets switch diagnostics on.
pl_add_job_library(pl_diagnostics ${PL_DIAGNOSTICS_SOURCES})
target_include_directories(pl_diagnostics PUBLIC libs/diagnostics/include)
target_link_libraries(pl_diagnostics PUBLIC pl_primitives pl_vocabulary)

# The static universe: people/PII, products, devices/IPs/rings,
# family and social graphs. One library, two layer trees (synth and
# relationships are mutually-dependent halves of world synthesis).
pl_add_job_library(pl_world
    ${PL_SYNTH_PII_SOURCES}
    ${PL_SYNTH_PRODUCTS_SOURCES}
    ${PL_SYNTH_INFRA_SOURCES}
    ${PL_RELATIONSHIPS_SOURCES}
)
target_include_directories(pl_world PUBLIC
    libs/synth/include
    libs/relationships/include
)
target_link_libraries(pl_world PUBLIC pl_primitives pl_diagnostics)
target_link_libraries(pl_world PRIVATE faker-cxx)

# Balance clearing: the authoritative ledger and LOC accrual.
pl_add_job_library(pl_ledger ${PL_TRANSACTIONS_SOURCES})
target_include_directories(pl_ledger PUBLIC libs/transactions/include)
target_link_libraries(pl_ledger PUBLIC pl_primitives pl_vocabulary)

# Day-to-day discretionary spending simulation (incl. its own
# spending-emission diagnostics). Consumes the transaction vocabulary
# (record/draft/factory) and the clearing book (soft screens + emission
# gate) by design — the 2026-07-19 lint scan amendment; see
# cmake/IncludeLayerLint.cmake. Card-cycle billing is consumed through
# the activity-owned CardCycleBilling interface (round C-1), so no
# transfers symbol is referenced here.
pl_add_job_library(pl_activity ${PL_ACTIVITY_SPENDING_SOURCES})
target_include_directories(pl_activity PUBLIC libs/activity/include)
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
target_include_directories(pl_transfers PUBLIC libs/transfers/include)
target_link_libraries(pl_transfers PUBLIC pl_activity pl_ledger)

# Orchestration: stages, chunking, the windowed streaming engine.
pl_add_job_library(pl_pipeline ${PL_PIPELINE_SOURCES})
target_include_directories(pl_pipeline PUBLIC libs/pipeline/include)
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
target_include_directories(pl_export PUBLIC libs/exporter/include)
target_link_libraries(pl_export PUBLIC pl_pipeline)
target_link_libraries(pl_export PRIVATE PostgreSQL::PostgreSQL)

# Binary surface: CLI parsing, progress reporting, run setup.
pl_add_job_library(pl_app ${PL_APP_SOURCES})
target_include_directories(pl_app PUBLIC libs/app/include)
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
# When clangd opens a project header (.hpp under libs/*/include/), it
# has no matching entry and falls back to whatever .cpp seems related —
# which, with faker-cxx in the dependency tree, lands on faker source
# files whose include paths don't cover our module include trees.
#
# Adding every .hpp to a real CMake target makes CMake emit explicit
# compile_commands entries for them with the project's include paths.
# clangd no longer has to guess.
#
# EXCLUDE_FROM_ALL keeps these out of `make` / `make build` so the
# normal build doesn't try to compile headers as translation units.
# The compile_commands entries are still emitted regardless.
# ---------------------------------------------------------------------

file(GLOB_RECURSE PL_PROJECT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/*.hpp"
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

add_executable(phantomledger libs/app/src/main.cpp)
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

# Every module's include tree merges into one installed phantomledger/
# prefix, exactly as the pre-L3 single include/ tree installed.
file(GLOB PL_MODULE_INCLUDE_DIRS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/*/include")
foreach(dir IN LISTS PL_MODULE_INCLUDE_DIRS)
    install(DIRECTORY "${dir}/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
    )
endforeach()
