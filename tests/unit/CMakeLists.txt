add_executable(test_order_pool test_order_pool.cpp)
target_link_libraries(test_order_pool PRIVATE meridian GTest::gtest_main)
target_compile_options(test_order_pool PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_order_pool)
