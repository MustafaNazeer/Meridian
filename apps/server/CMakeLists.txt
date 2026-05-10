# meridian-server: live demo binary with uWebSockets.
#
# Sources land here in Phase 8 (Engine Developer plus DevOps Engineer). Until then this
# stub is intentionally inert; the root CMakeLists.txt does not invoke
# add_subdirectory(apps/server) yet. When the Phase 8 dispatch lands main.cpp,
# sampler.cpp, and ws.cpp here, the dispatched agent uncomments the corresponding
# add_subdirectory line in the root CMakeLists.txt and adds a target definition
# (add_executable(meridian-server ...) plus the required link to libmeridian,
# uWebSockets, simdjson, and glaze).
