# Meridian test suite: unit, property-based, integration, concurrency.
#
# Sources land here in Phase 1 (QA Engineer plus Engine Developer) for the first
# unit tests on Order, Level, Book, OrderPool, and the matching loop. Property tests
# follow in Phase 3, integration tests in Phase 6, concurrency (TSAN) tests in Phase 4.
#
# Until Phase 1 lands the first test source, this stub is intentionally inert; the
# root CMakeLists.txt does not invoke add_subdirectory(tests) yet. When the Phase 1
# dispatch lands the first test, the dispatched agent uncomments the matching block
# in the root CMakeLists.txt and adds add_executable plus gtest_discover_tests for
# each test target here.
