# Meridian test suite: unit, property-based, integration, concurrency.

add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(property)
# add_subdirectory(concurrency)  # added when TSAN coverage is wired up
