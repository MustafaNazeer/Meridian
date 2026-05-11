# Concurrency tests: built with ThreadSanitizer to catch data races on
# the seqlock-protected snapshot. Runs as part of ctest on every PR.
#
# The test target compiles with -fsanitize=thread, -g, -O1. The library
# libmeridian.a is intentionally not linked: SeqlockSnapshot is header
# only, so the test binary is fully TSAN instrumented for the code under
# test. Linking gtest_main (compiled without TSAN) is fine because TSAN
# only watches code inside instrumented translation units.

add_executable(test_seqlock_concurrent test_seqlock_concurrent.cpp)

# Header-only access to SeqlockSnapshot and the shared types.
target_include_directories(test_seqlock_concurrent PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_seqlock_concurrent PRIVATE
    GTest::gtest_main
    Threads::Threads
)

target_compile_options(test_seqlock_concurrent PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -fsanitize=thread -g -O1
)

target_link_options(test_seqlock_concurrent PRIVATE
    -fsanitize=thread
)

gtest_discover_tests(test_seqlock_concurrent)

# test_seqlock_depth_concurrent is built WITHOUT ThreadSanitizer.
# SeqlockDepth deliberately uses a non-atomic struct copy under the
# seqlock retry contract (see include/meridian/depth_snapshot.hpp
# header comment and ADR-0005). TSAN would flag the inner write as
# a race even though the seqlock retry guarantees readers never see
# a torn snapshot at the caller boundary. This target verifies the
# algorithmic correctness without TSAN instrumenting the non-atomic
# copy.
add_executable(test_seqlock_depth_concurrent test_seqlock_depth_concurrent.cpp)
target_include_directories(test_seqlock_depth_concurrent PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_seqlock_depth_concurrent PRIVATE
    GTest::gtest_main
    Threads::Threads
)
target_compile_options(test_seqlock_depth_concurrent PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -g -O1
)
gtest_discover_tests(test_seqlock_depth_concurrent)
