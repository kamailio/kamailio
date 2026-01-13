file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
# TODO: https://github.com/kamailio/kamailio/issues/3618
pkg_check_modules(libmicrohttpd REQUIRED IMPORTED_TARGET libmicrohttpd>=0.9.72)
add_library(microhttpd::microhttpd ALIAS PkgConfig::libmicrohttpd)

target_link_libraries(${module_name} PRIVATE microhttpd::microhttpd)
