file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
add_library(systemd::systemd ALIAS PkgConfig::SYSTEMD)

target_link_libraries(${module_name} PRIVATE systemd::systemd)
