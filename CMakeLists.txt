cmake_minimum_required(VERSION 3.25)

project(meridian
    VERSION 0.1.0
    DESCRIPTION "Meridian: price-time priority limit order book and matching engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Default build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

option(MERIDIAN_BUILD_TESTS "Build the Meridian test suite" ON)
option(MERIDIAN_BUILD_BENCH "Build meridian-bench" ON)
option(MERIDIAN_BUILD_REPLAY "Build meridian-replay" ON)
option(MERIDIAN_BUILD_SERVER "Build meridian-server" ON)

include(FetchContent)

# GoogleTest is pulled in early so dependent targets across phases can use it.
# Locked to v1.15.2 (released 2024-07-31). Update only with explicit DevOps and Code Reviewer sign-off.
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

if(MERIDIAN_BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    include(GoogleTest)
endif()

# HDRHistogram-c powers the meridian-bench latency capture. Only fetched when
# the bench is being built. Locked to v0.11.8 (released 2024-04-04). Update
# only with explicit DevOps and Code Reviewer sign-off, mirroring the
# GoogleTest pinning policy above.
if(MERIDIAN_BUILD_BENCH)
    FetchContent_Declare(
        hdr_histogram
        GIT_REPOSITORY https://github.com/HdrHistogram/HdrHistogram_c.git
        GIT_TAG 0.11.8
        GIT_SHALLOW TRUE
    )
    set(HDR_HISTOGRAM_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(HDR_HISTOGRAM_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(HDR_HISTOGRAM_INSTALL_SHARED OFF CACHE BOOL "" FORCE)
    set(HDR_HISTOGRAM_INSTALL_STATIC OFF CACHE BOOL "" FORCE)
    set(HDR_HISTOGRAM_BUILD_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(hdr_histogram)
endif()

# libmeridian.a: the engine library lives under src/ and include/meridian/.
add_subdirectory(src)

# Driver binaries (apps/). Each subdirectory's CMakeLists.txt is a stub today; the
# add_subdirectory lines are intentionally commented out until each binary's main.cpp lands
# in the phase noted next to it. When the sources land, the dispatched agent uncomments
# the matching line.
#
# apps/bench:    Phase 1 skeleton, Phase 5 full benchmark (Performance Engineer)
# apps/replay:   Phase 6 (Engine Developer)
# apps/server:   Phase 8 (Engine Developer)
#
if(MERIDIAN_BUILD_BENCH)
    add_subdirectory(apps/bench)
endif()
# add_subdirectory(apps/replay)
# add_subdirectory(apps/server)

# Tests (Phase 1 onward).
if(MERIDIAN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
