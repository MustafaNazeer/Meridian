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

add_executable(test_matching test_matching.cpp)
target_link_libraries(test_matching PRIVATE meridian GTest::gtest_main)
target_compile_options(test_matching PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_matching)

add_executable(test_order_index test_order_index.cpp)
target_link_libraries(test_order_index PRIVATE meridian GTest::gtest_main)
target_compile_options(test_order_index PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_order_index)

add_executable(test_book_registry test_book_registry.cpp)
target_link_libraries(test_book_registry PRIVATE meridian GTest::gtest_main)
target_compile_options(test_book_registry PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_book_registry)

add_executable(test_seqlock_snapshot test_seqlock_snapshot.cpp)
target_link_libraries(test_seqlock_snapshot PRIVATE meridian GTest::gtest_main)
target_compile_options(test_seqlock_snapshot PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_seqlock_snapshot)

add_executable(test_itch test_itch.cpp)
target_link_libraries(test_itch PRIVATE meridian GTest::gtest_main)
target_compile_options(test_itch PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_itch)
