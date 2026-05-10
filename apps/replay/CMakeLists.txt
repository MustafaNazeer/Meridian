# meridian-replay: ITCH 5.0 replayer + tape generator.
#
# Two binaries land here:
#
#   * meridian-replay     reads a tape, drives the matching engine,
#                         writes JSON Lines audit output.
#   * meridian-tape-gen   emits a deterministic synthetic ITCH tape.
#                         Used by tests/integration/test_itch_replay.cpp
#                         to fabricate a fixture tape at test time.

add_executable(meridian-replay main.cpp)
target_link_libraries(meridian-replay PRIVATE meridian)
target_compile_options(meridian-replay PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(meridian-tape-gen tape_gen.cpp)
target_link_libraries(meridian-tape-gen PRIVATE meridian)
target_compile_options(meridian-tape-gen PRIVATE -Wall -Wextra -Wpedantic -Werror)
