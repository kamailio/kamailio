cmake_minimum_required(VERSION 3.10)

project(ndb_redis)

find_package(PkgConfig REQUIRED)
pkg_check_modules(redis REQUIRED IMPORTED_TARGET hiredis)
add_library(redis::redis ALIAS PkgConfig::redis)

file(GLOB SRC_FILES "*.c")

add_library(ndb_redis SHARED ${SRC_FILES})
target_link_libraries(ndb_redis PRIVATE kamailio redis::redis)
