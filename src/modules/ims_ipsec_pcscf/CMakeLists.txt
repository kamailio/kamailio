file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(libmnl REQUIRED IMPORTED_TARGET libmnl)
add_library(libmnl::libmnl ALIAS PkgConfig::libmnl)

find_package(Threads REQUIRED)

target_link_libraries(${module_name} PRIVATE libmnl::libmnl Threads::Threads)
