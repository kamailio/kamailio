cmake_minimum_required(VERSION 3.10)

project(group)

file(GLOB SRC_FILES "*.c")

add_library(group SHARED ${SRC_FILES})
target_link_libraries(group PRIVATE common_modules kamailio)

target_link_libraries(group PRIVATE srdb1)
set_target_properties(
  group PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
