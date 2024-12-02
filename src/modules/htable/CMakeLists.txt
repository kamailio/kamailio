cmake_minimum_required(VERSION 3.10)

project(htable)

file(GLOB SRC_FILES "*.c")

add_library(htable SHARED ${SRC_FILES})
target_link_libraries(htable PRIVATE common_modules kamailio)

target_link_libraries(htable PRIVATE srdb1)
set_target_properties(
  htable PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
