cmake_minimum_required(VERSION 3.10)

project(topos_redis)

find_package(PkgConfig REQUIRED)
pkg_check_modules(redis REQUIRED IMPORTED_TARGET hiredis)
add_library(redis::redis ALIAS PkgConfig::redis)

file(GLOB SRC_FILES "*.c")

add_library(topos_redis SHARED ${SRC_FILES})
target_link_libraries(topos_redis PRIVATE kamailio redis::redis)
