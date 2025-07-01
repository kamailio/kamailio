file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(expat REQUIRED IMPORTED_TARGET expat)
add_library(expat::expat ALIAS PkgConfig::expat)

target_link_libraries(${module_name} PRIVATE expat::expat)
