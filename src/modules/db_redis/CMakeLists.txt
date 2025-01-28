file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(redis REQUIRED IMPORTED_TARGET hiredis)
add_library(redis::redis ALIAS PkgConfig::redis)

# TODO: Check with Makefile for cluster stuff

target_link_libraries(${module_name} PRIVATE redis::redis)

include(${CMAKE_SOURCE_DIR}/cmake/dbschema.cmake)
add_db_target("${group_name}" db_redis "${STYLESHEETS}/db_redis.xsl")
