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

set(PL_ENTROPY_SOURCES
    src/entropy/crypto/blake2b.cpp
    src/entropy/random/rng.cpp
)

set(PL_PRIMITIVES_SOURCES
    src/primitives/time/almanac.cpp
    src/primitives/time/calendar.cpp
)

set(PL_TRANSACTIONS_SOURCES
    src/transactions/clearing/ledger.cpp
    src/transactions/clearing/loc_accrual.cpp
)

set(PL_ENTITIES_SOURCES
    src/entities/synth/pii/geonames.cpp
    src/entities/synth/pii/pools.cpp
)

set(PL_TRANSFERS_SOURCES
    src/transfers/credit_cards/cycle.cpp
    src/transfers/credit_cards/dispute.cpp
    src/transfers/credit_cards/lifecycle.cpp
    src/transfers/credit_cards/payment.cpp
    src/transfers/credit_cards/session.cpp
    src/transfers/credit_cards/statement.cpp
    src/transfers/government/disability.cpp
    src/transfers/government/retirement.cpp
    src/transfers/insurance/claims.cpp
    src/transfers/insurance/premiums.cpp
    src/transfers/obligations/schedule.cpp
    src/transfers/subscriptions/debits.cpp
)

set(PL_SPENDING_SOURCES
    src/spending/actors/day.cpp
    src/spending/dynamics/monthly/evolution.cpp
    src/spending/market/bootstrap.cpp
    src/spending/routing/payments.cpp
    src/spending/simulator/commerce_evolver.cpp
    src/spending/simulator/day_driver.cpp
    src/spending/simulator/day_source.cpp
    src/spending/simulator/driver.cpp
    src/spending/simulator/loop.cpp
    src/spending/simulator/population_dynamics.cpp
    src/spending/simulator/run.cpp
    src/spending/simulator/run_planner.cpp
    src/spending/simulator/spender_emission_driver.cpp
    src/spending/spenders/prepare.cpp
    src/spending/spenders/targets.cpp
)

set(PL_FRAUD_SOURCES
    src/transfers/fraud/camouflage.cpp
    src/transfers/fraud/engine.cpp
    src/transfers/fraud/injector.cpp
    src/transfers/fraud/rings.cpp
    src/transfers/fraud/schedule.cpp
    src/transfers/fraud/typologies/classic.cpp
    src/transfers/fraud/typologies/dispatch.cpp
    src/transfers/fraud/typologies/funnel.cpp
    src/transfers/fraud/typologies/invoice.cpp
    src/transfers/fraud/typologies/layering.cpp
    src/transfers/fraud/typologies/mule.cpp
    src/transfers/fraud/typologies/structuring.cpp
)

set(PL_PRODUCTS_SOURCES
    src/entities/synth/products/amount_sampling.cpp
    src/entities/synth/products/auto_loan.cpp
    src/entities/synth/products/dates.cpp
    src/entities/synth/products/installment_emission.cpp
    src/entities/synth/products/insurance.cpp
    src/entities/synth/products/mortgage.cpp
    src/entities/synth/products/obligation_emission.cpp
    src/entities/synth/products/student_loan.cpp
    src/entities/synth/products/tax.cpp
)

set(PL_LEGIT_SOURCES
    src/transfers/legit/blueprints/models.cpp
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
)

set(PL_RELATIONSHIPS_SOURCES
    src/relationships/family/builder.cpp
    src/relationships/family/links.cpp
    src/relationships/family/partition.cpp
    src/relationships/family/support.cpp
    src/relationships/social/builder.cpp
    src/relationships/social/communities.cpp
    src/relationships/social/sampler.cpp
)

set(PL_RUN_SOURCES
    src/run/progress.cpp
)

set(PL_EXPORTER_SOURCES
    src/exporter/csv.cpp
    src/exporter/standard/export.cpp
    src/exporter/mule_ml/canonical.cpp
    src/exporter/mule_ml/export.cpp
    src/exporter/aml/minhash.cpp
    src/exporter/aml/identity.cpp
    src/exporter/aml/sar.cpp
    src/exporter/aml/vertices.cpp
    src/exporter/aml/edges.cpp
    src/exporter/aml/export.cpp
)

set(PL_INFRA_SYNTH_SOURCES
    src/infra/synth/devices.cpp
    src/infra/synth/ips.cpp
    src/infra/synth/rings.cpp
)

set(PL_PIPELINE_SOURCES
    src/pipeline/simulate.cpp
    src/pipeline/stages/entities.cpp
    src/pipeline/stages/infra.cpp
    src/pipeline/stages/transfers.cpp
)

set(PL_ALL_SOURCES
    ${PL_ENTROPY_SOURCES}
    ${PL_PRIMITIVES_SOURCES}
    ${PL_TRANSACTIONS_SOURCES}
    ${PL_ENTITIES_SOURCES}
    ${PL_TRANSFERS_SOURCES}
    ${PL_SPENDING_SOURCES}
    ${PL_FRAUD_SOURCES}
    ${PL_PRODUCTS_SOURCES}
    ${PL_LEGIT_SOURCES}
    ${PL_RELATIONSHIPS_SOURCES}
    ${PL_RUN_SOURCES}
    ${PL_EXPORTER_SOURCES}
    ${PL_INFRA_SYNTH_SOURCES}
    ${PL_PIPELINE_SOURCES}
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

list(REMOVE_ITEM PL_DISCOVERED_SOURCES_REL "src/main.cpp")

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
# Library target
# ---------------------------------------------------------------------

add_library(phantomledger_core STATIC ${PL_ALL_SOURCES})

target_include_directories(phantomledger_core PUBLIC include)
target_compile_features(phantomledger_core PUBLIC cxx_std_23)
target_link_libraries(phantomledger_core PUBLIC faker-cxx)

if(MSVC)
    target_compile_options(phantomledger_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(phantomledger_core PRIVATE -Wall -Wextra -Wpedantic)

    if(PL_ENABLE_NATIVE_OPTIMIZATION)
        target_compile_options(phantomledger_core PRIVATE -march=native)
    endif()
endif()

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
    # Inherit phantomledger_core's include paths and compile features
    # so the entries match real build flags exactly.
    target_link_libraries(phantomledger_headers PRIVATE phantomledger_core)
endif()

# ---------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------

add_executable(phantomledger src/main.cpp)
target_link_libraries(phantomledger PRIVATE phantomledger_core)

if(MSVC)
    target_compile_options(phantomledger PRIVATE /W4 /permissive-)
else()
    target_compile_options(phantomledger PRIVATE -Wall -Wextra -Wpedantic)

    if(PL_ENABLE_NATIVE_OPTIMIZATION)
        target_compile_options(phantomledger PRIVATE -march=native)
    endif()
endif()

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

install(TARGETS phantomledger_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)
