cmake_minimum_required(VERSION 3.10)

project(json)

file(GLOB SRC_FILES "*.c")

find_package(json-c QUIET)
if(NOT json-c_FOUND)
  message(STATUS "json-c not found. looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(json-c REQUIRED IMPORTED_TARGET json-c)
  add_library(json-c::json-c ALIAS PkgConfig::json-c)
endif()

add_library(json SHARED ${SRC_FILES})
target_link_libraries(json PRIVATE kamailio json-c::json-c)
