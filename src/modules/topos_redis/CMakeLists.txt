file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# tps_common.c (symlink to ../topos) includes topos headers
target_include_directories(${module_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../topos")

find_package(PkgConfig REQUIRED)
pkg_check_modules(hiredis REQUIRED IMPORTED_TARGET hiredis)
add_library(hiredis::hiredis ALIAS PkgConfig::hiredis)

target_link_libraries(${module_name} PRIVATE hiredis::hiredis)
