# meridian-server: live demo binary with uWebSockets.
#
# Sources land here when the WebSocket server is wired up. Until then this
# stub is intentionally inert; the root CMakeLists.txt does not invoke
# add_subdirectory(apps/server) yet. When main.cpp, sampler.cpp, and ws.cpp
# land here, uncomment the corresponding add_subdirectory line in the root
# CMakeLists.txt and add a target definition (add_executable(meridian-server
# ...) plus the required link to libmeridian, uWebSockets, simdjson, and
# glaze).
