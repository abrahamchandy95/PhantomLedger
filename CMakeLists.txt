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
set(PL_FRAUD_SOURCES
    src/transfers/fraud/engine.cpp
    src/transfers/fraud/rings.cpp
    src/transfers/fraud/schedule.cpp
    src/transfers/fraud/camouflage.cpp
    src/transfers/fraud/injector.cpp
    src/transfers/fraud/typologies/classic.cpp
    src/transfers/fraud/typologies/dispatch.cpp
    src/transfers/fraud/typologies/funnel.cpp
    src/transfers/fraud/typologies/invoice.cpp
    src/transfers/fraud/typologies/layering.cpp
    src/transfers/fraud/typologies/mule.cpp
    src/transfers/fraud/typologies/structuring.cpp
)

set(PL_LEGIT_SOURCES
    src/transfers/legit/blueprints/paydays.cpp
    src/transfers/legit/blueprints/plans.cpp
    src/transfers/legit/blueprints/models.cpp
    src/transfers/legit/ledger/screenbook.cpp
    src/transfers/legit/ledger/streams.cpp
    src/transfers/legit/ledger/burdens.cpp
    src/transfers/legit/ledger/limits.cpp
    src/transfers/legit/ledger/posting.cpp
    src/transfers/legit/ledger/builder.cpp
    src/transfers/legit/ledger/passes.cpp
    src/transfers/legit/routines/credit_cards.cpp
    src/transfers/legit/routines/paychecks.cpp
    src/transfers/legit/routines/relatives.cpp
    src/transfers/legit/routines/family/allowances.cpp
    src/transfers/legit/routines/family/support.cpp
    src/transfers/legit/routines/family/tuition.cpp
    src/transfers/legit/routines/family/parent_gifts.cpp
    src/transfers/legit/routines/family/siblings.cpp
    src/transfers/legit/routines/family/grandparent_gifts.cpp
    src/transfers/legit/routines/family/spouse.cpp
    src/transfers/legit/routines/family/inheritance.cpp
    src/transfers/legit/routines/atm.cpp
    src/transfers/legit/routines/internal.cpp
    src/transfers/legit/routines/spending.cpp
    src/transfers/legit/routines/subscriptions.cpp
)

set(PL_RELATIONSHIPS_SOURCES
    src/relationships/family/partition.cpp
    src/relationships/family/links.cpp
    src/relationships/family/support.cpp
    src/relationships/family/builder.cpp
    src/relationships/social/communities.cpp
    src/relationships/social/sampler.cpp
    src/relationships/social/builder.cpp
)

set(PL_EXPORTER_SOURCES
    src/exporter/csv.cpp
)

set(PL_INFRA_SYNTH_SOURCES
    src/infra/synth/rings.cpp
    src/infra/synth/devices.cpp
    src/infra/synth/ips.cpp
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
    ${PL_FRAUD_SOURCES}
    ${PL_LEGIT_SOURCES}
    ${PL_RELATIONSHIPS_SOURCES}
    ${PL_EXPORTER_SOURCES}
    ${PL_INFRA_SYNTH_SOURCES}
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
