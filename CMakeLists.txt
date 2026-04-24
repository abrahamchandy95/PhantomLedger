cmake_minimum_required(VERSION 3.23)
project(phantomledger VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(PL_BUILD_TESTS  "Build C++ tests" ON)
option(PL_BUILD_PYTHON "Build pybind11 Python module" OFF)

add_library(phantomledger_core STATIC
    src/transactions/clearing/ledger.cpp
    src/entropy/crypto/blake2b.cpp
    src/primitives/time/calendar.cpp
    src/entropy/random/rng.cpp
)

target_include_directories(phantomledger_core PUBLIC include)
target_compile_features(phantomledger_core PUBLIC cxx_std_23)

target_compile_options(phantomledger_core PRIVATE
    -Wall -Wextra -Wpedantic -O3 -march=native
)

if(PL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
