find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
add_library(systemd::systemd ALIAS PkgConfig::SYSTEMD)

file(GLOB SRC_FILES "*.c")

add_library(systemdops SHARED ${SRC_FILES})
target_link_libraries(systemdops PRIVATE systemd::systemd)
