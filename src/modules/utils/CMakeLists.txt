cmake_minimum_required(VERSION 3.10)

project(utils)

file(GLOB SRC_FILES "*.c")

add_library(utils SHARED ${SRC_FILES})
target_link_libraries(utils PRIVATE common_modules kamailio)

target_link_libraries(utils PRIVATE srdb1)
set_target_properties(
  utils PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
