find_package(PkgConfig REQUIRED)
pkg_check_modules(redis REQUIRED IMPORTED_TARGET hiredis)
add_library(redis::redis ALIAS PkgConfig::redis)

file(GLOB SRC_FILES "*.c")

add_library(topos_redis SHARED ${SRC_FILES})
target_link_libraries(topos_redis PRIVATE redis::redis)

target_compile_options(
  topos_redis
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/topos_redis/=")
set_target_properties(topos_redis PROPERTIES SUFFIX ".so")
