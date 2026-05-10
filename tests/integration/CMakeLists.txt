add_executable(test_engine_vs_reference test_engine_vs_reference.cpp)

target_compile_definitions(test_engine_vs_reference PRIVATE
    SOURCE_REFERENCE_DIR="${CMAKE_SOURCE_DIR}/tests/reference"
)

target_link_libraries(test_engine_vs_reference PRIVATE
    meridian
    GTest::gtest_main
)

target_compile_options(test_engine_vs_reference PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

gtest_discover_tests(test_engine_vs_reference)

add_executable(test_multi_instrument test_multi_instrument.cpp)

target_compile_definitions(test_multi_instrument PRIVATE
    SOURCE_REFERENCE_DIR="${CMAKE_SOURCE_DIR}/tests/reference"
)

target_link_libraries(test_multi_instrument PRIVATE
    meridian
    GTest::gtest_main
)

target_compile_options(test_multi_instrument PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

gtest_discover_tests(test_multi_instrument)

# ITCH replay diff: builds against meridian-replay and meridian-tape-gen
# so the test can shell out to them at run time. The paths are
# generator-expressions resolved at build time.
add_executable(test_itch_replay test_itch_replay.cpp)

target_compile_definitions(test_itch_replay PRIVATE
    SOURCE_REFERENCE_DIR="${CMAKE_SOURCE_DIR}/tests/reference"
    MERIDIAN_REPLAY_PATH="$<TARGET_FILE:meridian-replay>"
    MERIDIAN_TAPE_GEN_PATH="$<TARGET_FILE:meridian-tape-gen>"
)

target_link_libraries(test_itch_replay PRIVATE
    GTest::gtest_main
)

target_compile_options(test_itch_replay PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

add_dependencies(test_itch_replay meridian-replay meridian-tape-gen)

gtest_discover_tests(test_itch_replay)
