file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(nats REQUIRED IMPORTED_TARGET libnats)
add_library(nats::libnats ALIAS PkgConfig::nats)
pkg_check_modules(uv REQUIRED IMPORTED_TARGET libuv)
add_library(uv::libuv ALIAS PkgConfig::uv)

target_link_libraries(${module_name} PRIVATE nats::libnats uv::libuv)
