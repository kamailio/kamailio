find_package(PkgConfig REQUIRED)
pkg_check_modules(redis REQUIRED IMPORTED_TARGET hiredis)
add_library(redis::redis ALIAS PkgConfig::redis)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EVENT REQUIRED IMPORTED_TARGET libevent)
add_library(event::event ALIAS PkgConfig::EVENT)

file(GLOB SRC_FILES "*.c")

add_library(cnxcc SHARED ${SRC_FILES})
target_link_libraries(cnxcc PRIVATE redis::redis event::event)

target_compile_options(
  cnxcc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/cnxcc/=")
set_target_properties(cnxcc PROPERTIES SUFFIX ".so")
