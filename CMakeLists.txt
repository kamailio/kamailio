cmake_minimum_required(VERSION 3.10)

# Use cmake -DCMAKE_SYSTEM_NAME .. for cross-compiling (inside build directory)

# cmake_policy must be before project for working with toolchain together
if(WIN32)
  cmake_policy(SET CMP0074 NEW)
  cmake_policy(SET CMP0156 OLD)
  cmake_policy(SET CMP0082 OLD)
endif()

# Set the project name
project(
  kamailio
  VERSION 6.0.0
  DESCRIPTION "Kamailio SIP Server"
  HOMEPAGE_URL "https://www.kamailio.org")

# Set the version number
set(EXTRAVERSION "-pre0")
set(RELEASE "${PROJECT_VERSION}${EXTRAVERSION}")

message(STATUS "PROJECT_VERSION: ${PROJECT_VERSION}")
message(STATUS "RELEASE:  ${RELEASE}")

# cmake-format: off
# Set the version number as a single integer
math(EXPR VERSIONVAL
  "${PROJECT_VERSION_MAJOR}*1000000
    + ${PROJECT_VERSION_MINOR}*1000
    + ${PROJECT_VERSION_PATCH}"
)
message(STATUS "VERSIONVAL: ${VERSIONVAL}")
# cmake-format: on

# Specify the C standard if non provided by user
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED True)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(${CMAKE_SOURCE_DIR}/cmake/BuildType.cmake)
# -----------------------
# Main project name
# -----------------------
# main binary name
set(MAIN_NAME
    "kamailio"
    CACHE STRING "Main binary name")

# use kamailio config
set(CFG_NAME
    "kamailio"
    CACHE STRING "Config name")

include(GNUInstallDirs)
include(${CMAKE_SOURCE_DIR}/cmake/defs.cmake)

# Add the source directory
add_subdirectory(src)

# Add utils
# add_subdirectory(utils)
if(NOT WIN32)
  add_subdirectory(utils/kamctl)
  add_subdirectory(utils/kamcmd)
endif()

# TODO: Packaging stuuf. These should be on different file propably
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_NAME kamailio)
if(NOT WIN32)
  set(CPACK_GENERATOR DEB)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "test")
endif()

include(CPack)

# Add uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake-uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake-uninstall.cmake" IMMEDIATE @ONLY)

  add_custom_target(
    uninstall
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake-uninstall.cmake
    COMMENT "Uninstalling")
endif()
