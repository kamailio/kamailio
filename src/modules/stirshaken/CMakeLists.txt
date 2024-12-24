file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(stirshaken REQUIRED IMPORTED_TARGET libstireshaken)
add_library(stirshaken::stirshaken ALIAS PkgConfig::stirshaken)

target_link_libraries(${module_name} PRIVATE stirshaken::stirshaken)
