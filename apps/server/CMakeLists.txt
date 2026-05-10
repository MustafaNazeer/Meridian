# meridian-server: live demo binary.
#
# The WebSocket server lives in libmeridian.a (src/ws_server.cpp); this
# directory wires the engine + sampler + ws threads together in a
# single binary. Hand rolled WS server, no external networking library.

add_executable(meridian-server main.cpp)
target_link_libraries(meridian-server PRIVATE meridian Threads::Threads)
target_compile_options(meridian-server PRIVATE -Wall -Wextra -Wpedantic -Werror)
