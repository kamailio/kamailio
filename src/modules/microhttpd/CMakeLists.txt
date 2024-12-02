cmake_minimum_required(VERSION 3.10)

project(microhttpd)

find_package(PkgConfig REQUIRED)
# TODO: https://github.com/kamailio/kamailio/issues/3618
pkg_check_modules(libmicrohttpd REQUIRED IMPORTED_TARGET libmicrohttpd>=0.9.75)
add_library(microhttpd::microhttpd ALIAS PkgConfig::libmicrohttpd)

file(GLOB SRC_FILES "*.c")

add_library(microhttpd SHARED ${SRC_FILES})
target_link_libraries(microhttpd PRIVATE kamailio microhttpd::microhttpd)
