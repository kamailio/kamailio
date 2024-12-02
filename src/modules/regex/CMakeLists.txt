cmake_minimum_required(VERSION 3.10)

project(regex)

find_package(pcre2 QUIET)
if(NOT pcre2_FOUND)
  message(WARNING "PCRE2 library not found... looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  #   TODO: verify we want 8-bit libpcre2
  pkg_check_modules(pcre2 REQUIRED IMPORTED_TARGET libpcre2-8)
  add_library(PCRE2::8BIT ALIAS PkgConfig::pcre2)
endif()

file(GLOB SRC_FILES "*.c")

add_library(regex SHARED ${SRC_FILES})
target_link_libraries(regex PRIVATE kamailio PCRE2::8BIT)
