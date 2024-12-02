cmake_minimum_required(VERSION 3.10)

project(topos)

file(GLOB SRC_FILES "*.c")

add_library(topos SHARED ${SRC_FILES})
target_link_libraries(topos PRIVATE common_modules kamailio)

target_link_libraries(topos PRIVATE srdb1)
set_target_properties(
  topos PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
