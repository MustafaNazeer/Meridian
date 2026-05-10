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
