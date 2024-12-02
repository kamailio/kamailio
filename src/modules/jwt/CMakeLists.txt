cmake_minimum_required(VERSION 3.10)

project(jwt)

file(GLOB SRC_FILES "*.c")

find_package(libjwt 1.12 QUIET)
if(NOT libjwt_FOUND)
  message(STATUS "libjwt not found... looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(jwt REQUIRED IMPORTED_TARGET libjwt>=1.12)
  add_library(jwt::libjwt ALIAS PkgConfig::libjwt)
endif()

add_library(jwt SHARED ${SRC_FILES})
target_link_libraries(jwt PRIVATE kamailio jwt::libjwt)
