# meridian-replay: CLI ITCH replayer that streams JSON Lines to stdout.
#
# Sources land here when the ITCH replayer is wired up. Until then this
# stub is intentionally inert; the root CMakeLists.txt does not invoke
# add_subdirectory(apps/replay) yet. When main.cpp lands here, uncomment
# the corresponding add_subdirectory line in the root CMakeLists.txt and
# add a target definition (add_executable(meridian-replay main.cpp) plus
# the required link to libmeridian and the ITCH parser header).
