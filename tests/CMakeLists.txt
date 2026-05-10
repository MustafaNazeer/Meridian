# Meridian test suite: unit, property-based, integration, concurrency.

# Threads::Threads is required by tests/concurrency/.
find_package(Threads REQUIRED)

add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(property)
add_subdirectory(concurrency)
