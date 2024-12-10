file(GLOB SRC_FILES "*.c")

find_package(PkgConfig)
pkg_check_modules(LIB_WEBSOCKETS REQUIRED IMPORTED_TARGET libwebsockets)
add_library(libws::libwebsockets ALIAS PkgConfig::LIB_WEBSOCKETS)

add_library(lwsc SHARED ${SRC_FILES})
target_link_libraries(lwsc PRIVATE libws::libwebsockets)
