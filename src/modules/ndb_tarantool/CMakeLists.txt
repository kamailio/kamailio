file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_search_module(msgpack REQUIRED IMPORTED_TARGET msgpack-c msgpack msgpackc)
add_library(msgpack::msgpack ALIAS PkgConfig::msgpack)

target_link_libraries(${module_name} PRIVATE msgpack::msgpack)
