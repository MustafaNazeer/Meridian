# meridian-bench: zero-I/O throughput and latency benchmark.
#
# Sources land here in Phase 5 (Performance Engineer). Until then this stub is
# intentionally inert; the root CMakeLists.txt does not invoke add_subdirectory(apps/bench)
# yet. When the Phase 5 dispatch lands main.cpp here, the dispatched agent uncomments
# the corresponding add_subdirectory line in the root CMakeLists.txt and adds a target
# definition (add_executable(meridian-bench main.cpp) plus the required link to
# libmeridian and Google Benchmark).
