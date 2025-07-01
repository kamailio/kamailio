file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(memcached REQUIRED IMPORTED_TARGET libmemcached)
add_library(memcached::memcached ALIAS PkgConfig::memcached)

target_link_libraries(${module_name} PRIVATE memcached::memcached)
