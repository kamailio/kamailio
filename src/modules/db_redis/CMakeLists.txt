find_package(PkgConfig REQUIRED)
pkg_check_modules(redis REQUIRED IMPORTED_TARGET hiredis)
add_library(redis::redis ALIAS PkgConfig::redis)

file(GLOB SRC_FILES "*.c")

# TODO: Check with Makefile for cluster stuff

add_library(db_redis SHARED ${SRC_FILES})

target_link_libraries(db_redis PRIVATE redis::redis)

target_compile_options(db_redis PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db_redis/=")
