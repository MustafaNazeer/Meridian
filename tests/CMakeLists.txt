# Meridian test suite: unit, property-based, integration, concurrency.

add_subdirectory(unit)
# add_subdirectory(integration)  # uncommented when Task 14 lands the diff harness
# add_subdirectory(property)     # added in Phase 3 with rapidcheck
# add_subdirectory(concurrency)  # added in Phase 4 with TSAN coverage
