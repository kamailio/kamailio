cmake_minimum_required(VERSION 3.10)

# Use cmake -DCMAKE_SYSTEM_NAME .. for cross-compiling (inside build directory)

# Set the project name
project(kamailio VERSION 6.0.0)

# Set the version number
set(EXTRAVERSION "-dev2")
set(RELEASE "${PROJECT_VERSION}${EXTRAVERSION}")

message(STATUS "PROJECT_VERSION: ${PROJECT_VERSION}")
message(STATUS "RELEASE:  ${RELEASE}")

math(
  EXPR
  VERSIONVAL
  "${PROJECT_VERSION_MAJOR}*1000000 + ${PROJECT_VERSION_MINOR}*1000 + ${PROJECT_VERSION_PATCH}"
)
message(STATUS "VERSIONVAL: ${VERSIONVAL}")

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# -----------------------
# Main project name
# -----------------------
# main binary name
set(MAIN_NAME "kamailio")
# use kamailio config
set(CFG_NAME "kamailio")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

# Add the source directory
add_subdirectory(src)

# Add utils
add_subdirectory(utils/kamctl)

# Add uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake" IMMEDIATE @ONLY
  )

  add_custom_target(
    uninstall COMMAND ${CMAKE_COMMAND} -P
                      ${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake
  )
endif()
