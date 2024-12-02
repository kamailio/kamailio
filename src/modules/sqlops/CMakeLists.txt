cmake_minimum_required(VERSION 3.10)

project(sqlops)

file(GLOB SRC_FILES "*.c")

add_library(sqlops SHARED ${SRC_FILES})
target_link_libraries(sqlops PRIVATE common_modules kamailio)

target_link_libraries(sqlops PRIVATE srdb1)
set_target_properties(
  sqlops PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
