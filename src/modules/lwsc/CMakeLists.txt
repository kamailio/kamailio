file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig)
pkg_check_modules(LIB_WEBSOCKETS REQUIRED IMPORTED_TARGET libwebsockets)
add_library(libws::libwebsockets ALIAS PkgConfig::LIB_WEBSOCKETS)

target_link_libraries(${module_name} PRIVATE libws::libwebsockets)
