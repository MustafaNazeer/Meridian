# meridian-replay: CLI ITCH replayer that streams JSON Lines to stdout.
#
# Sources land here in Phase 6 (Engine Developer). Until then this stub is
# intentionally inert; the root CMakeLists.txt does not invoke add_subdirectory(apps/replay)
# yet. When the Phase 6 dispatch lands main.cpp here, the dispatched agent uncomments
# the corresponding add_subdirectory line in the root CMakeLists.txt and adds a target
# definition (add_executable(meridian-replay main.cpp) plus the required link to
# libmeridian and the ITCH parser header).
