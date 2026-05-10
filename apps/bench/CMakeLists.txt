# meridian-bench: zero-I/O throughput and latency benchmark.
#
# Phase 1 ships a working harness that compiles and runs end to end against
# libmeridian.a. The Markdown summary it prints is informational; the headline
# 6M events/sec target is enforced in Phase 5 via bench/baseline.json.
#
# HDRHistogram-c is wired in the root CMakeLists.txt (FetchContent) inside
# the MERIDIAN_BUILD_BENCH guard, so hdr_histogram_static is only available
# when the bench is being built.

add_executable(meridian-bench main.cpp)

target_link_libraries(meridian-bench PRIVATE
    meridian
    hdr_histogram_static
)

target_compile_options(meridian-bench PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
