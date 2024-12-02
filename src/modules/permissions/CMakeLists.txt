cmake_minimum_required(VERSION 3.10)

project(permissions)

file(GLOB SRC_FILES "*.c")

add_library(permissions SHARED ${SRC_FILES})
target_link_libraries(permissions PRIVATE common_modules kamailio)

target_link_libraries(permissions PRIVATE srdb1)
set_target_properties(
  permissions PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
