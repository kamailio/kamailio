cmake_minimum_required(VERSION 3.10)

project(keepalive)

file(GLOB SRC_FILES "*.c")

add_library(keepalive SHARED ${SRC_FILES})
target_link_libraries(keepalive PRIVATE common_modules kamailio)

target_link_libraries(keepalive PRIVATE srdb1)
set_target_properties(
  keepalive PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
