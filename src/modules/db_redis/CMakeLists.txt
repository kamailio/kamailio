cmake_minimum_required(VERSION 3.10)

project(db_redis)

find_package(PkgConfig REQUIRED)
pkg_check_modules(redis REQUIRED IMPORTED_TARGET hiredis)
add_library(redis::redis ALIAS PkgConfig::redis)

file(GLOB SRC_FILES "*.c")

# TODO: Check with Makefile for cluster stuff

add_library(db_redis SHARED ${SRC_FILES})
# target_include_directories(db_redis PRIVATE ${redis_INCLUDE_DIRS})
# target_compile_options(db_redis PRIVATE ${redis_CFLAGS})
# target_link_options(db_redis PRIVATE ${redis_LDFLAGS})
# target_link_libraries(db_redis PRIVATE kamailio ${redis_LIBRARIES})
# Link with the imported target should also make available the include directories and compile options
target_link_libraries(db_redis PRIVATE kamailio redis::redis)

target_link_libraries(db_redis PRIVATE srdb1 srdb2)
