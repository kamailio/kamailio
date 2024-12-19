find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
add_library(systemd::systemd ALIAS PkgConfig::SYSTEMD)

file(GLOB SRC_FILES "*.c")

add_library(systemdops SHARED ${SRC_FILES})
target_link_libraries(systemdops PRIVATE systemd::systemd)

target_compile_options(
  systemdops
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/systemdops/=")
set_target_properties(systemdops PROPERTIES SUFFIX ".so")
