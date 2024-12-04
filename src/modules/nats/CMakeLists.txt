cmake_minimum_required(VERSION 3.10)

project(nats)

find_package(PkgConfig REQUIRED)
pkg_check_modules(nats REQUIRED IMPORTED_TARGET libnats)
add_library(nats::libnats ALIAS PkgConfig::nats)
pkg_check_modules(uv REQUIRED IMPORTED_TARGET libuv)
add_library(uv::libuv ALIAS PkgConfig::uv)

file(GLOB SRC_FILES "*.c")

add_library(nats SHARED ${SRC_FILES})
target_link_libraries(nats PRIVATE kamailio nats::libnats uv::libuv)
