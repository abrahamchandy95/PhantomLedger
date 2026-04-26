cmake_minimum_required(VERSION 3.23)
project(phantomledger VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------
# Global build settings
# ---------------------------------------------------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(PL_BUILD_TESTS  "Build C++ tests"            ON)
option(PL_BUILD_PYTHON "Build pybind11 Python module" OFF)

# ---------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------

include(FetchContent)

set(FAKER_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING       OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    faker-cxx
    GIT_REPOSITORY https://github.com/cieslarmichal/faker-cxx.git
    GIT_TAG        v4.3.2
)
FetchContent_MakeAvailable(faker-cxx)

# Re-enable testing so our own tests subdirectory can register cases.
set(BUILD_TESTING ON CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------
# Source lists, grouped by subsystem.
# Keep these in alphabetical order within each group.
# ---------------------------------------------------------------------

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
    src/spending/simulator/day.cpp
    src/spending/simulator/loop.cpp
    src/spending/simulator/driver.cpp
    src/spending/simulator/run.cpp
    src/spending/spenders/prepare.cpp
    src/spending/spenders/targets.cpp
)

# ---------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------

add_library(phantomledger_core STATIC
    ${PL_ENTROPY_SOURCES}
    ${PL_PRIMITIVES_SOURCES}
    ${PL_TRANSACTIONS_SOURCES}
    ${PL_ENTITIES_SOURCES}
    ${PL_TRANSFERS_SOURCES}
    ${PL_SPENDING_SOURCES}
)

target_include_directories(phantomledger_core PUBLIC include)
target_compile_features(phantomledger_core PUBLIC cxx_std_23)

target_compile_options(phantomledger_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -O3
    -march=native
)

target_link_libraries(phantomledger_core PUBLIC faker-cxx)

# ---------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------

if(PL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
