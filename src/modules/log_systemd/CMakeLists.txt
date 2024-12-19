file(GLOB SRC_FILES "*.c")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
add_library(systemd::systemd ALIAS PkgConfig::SYSTEMD)

add_library(log_systemd SHARED ${SRC_FILES})
target_link_libraries(log_systemd PRIVATE systemd::systemd)

target_compile_options(
  log_systemd
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/log_systemd/=")
set_target_properties(log_systemd PROPERTIES SUFFIX ".so")
