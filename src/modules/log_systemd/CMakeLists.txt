cmake_minimum_required(VERSION 3.10)

project(log_systemd)

file(GLOB SRC_FILES "*.c")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
add_library(systemd::systemd ALIAS PkgConfig::SYSTEMD)

add_library(log_systemd SHARED ${SRC_FILES})
target_link_libraries(log_systemd PRIVATE kamailio systemd::systemd)
