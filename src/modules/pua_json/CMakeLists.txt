cmake_minimum_required(VERSION 3.10)

project(pua_json)

find_package(json-c QUIET)
if(NOT json-c_FOUND)
  message(WARNING "json-c not found. looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(json-c REQUIRED IMPORTED_TARGET json-c)
  add_library(json-c::json-c ALIAS PkgConfig::json-c)
endif()

file(GLOB SRC_FILES "*.c")

add_library(pua_json SHARED ${SRC_FILES})
target_link_libraries(pua_json PRIVATE kamailio json-c::json-c)
