add_library(meridian STATIC
    order_pool.cpp
    level.cpp
    book.cpp
    book_registry.cpp
    order_index.cpp
    matching.cpp
    ws_server.cpp
)

target_include_directories(meridian PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(meridian PUBLIC cxx_std_20)

target_compile_options(meridian PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
