file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(zlib REQUIRED IMPORTED_TARGET zlib)
add_library(zlib::zlib ALIAS PkgConfig::zlib)

target_link_libraries(${module_name} PRIVATE zlib::zlib)
