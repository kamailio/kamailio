file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(hiredis REQUIRED IMPORTED_TARGET hiredis)
add_library(hiredis::hiredis ALIAS PkgConfig::hiredis)

target_link_libraries(${module_name} PRIVATE hiredis::hiredis)
