cmake_minimum_required(VERSION 3.10)

project(sca)

file(GLOB SRC_FILES "*.c")

add_library(sca SHARED ${SRC_FILES})
target_link_libraries(sca PRIVATE common_modules kamailio)

target_link_libraries(sca PRIVATE srdb1)
set_target_properties(
  sca PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
