cmake_minimum_required(VERSION 3.10)

project(uuid)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
add_library(uuid::uuid ALIAS PkgConfig::UUID)

file(GLOB SRC_FILES "*.c")

add_library(uuid SHARED ${SRC_FILES})
target_link_libraries(uuid PRIVATE kamailio uuid::uuid)
