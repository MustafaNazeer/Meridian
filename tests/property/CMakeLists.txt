# Property-based tests: deterministic random event sequences run through
# the matching engine, with each invariant from
# docs/risk/matching-semantics.md section 9 checked across at least 1000
# cases per CI run. Generators and shadow-state bookkeeping are hand
# rolled in GoogleTest; no external PBT framework is pulled in.

add_executable(test_invariants
    test_invariants.cpp
)

target_link_libraries(test_invariants PRIVATE
    meridian
    GTest::gtest_main
)

target_compile_options(test_invariants PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

gtest_discover_tests(test_invariants)

add_executable(test_differential
    test_differential.cpp
)

target_compile_definitions(test_differential PRIVATE
    SOURCE_REFERENCE_DIR="${CMAKE_SOURCE_DIR}/tests/reference"
)

target_link_libraries(test_differential PRIVATE
    meridian
    GTest::gtest_main
)

target_compile_options(test_differential PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

gtest_discover_tests(test_differential)
