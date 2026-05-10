add_executable(test_order_pool test_order_pool.cpp)
target_link_libraries(test_order_pool PRIVATE meridian GTest::gtest_main)
target_compile_options(test_order_pool PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_order_pool)

add_executable(test_level test_level.cpp)
target_link_libraries(test_level PRIVATE meridian GTest::gtest_main)
target_compile_options(test_level PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_level)

add_executable(test_book test_book.cpp)
target_link_libraries(test_book PRIVATE meridian GTest::gtest_main)
target_compile_options(test_book PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_book)
