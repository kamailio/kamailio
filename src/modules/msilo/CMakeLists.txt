cmake_minimum_required(VERSION 3.10)

project(msilo)

file(GLOB SRC_FILES "*.c")

add_library(msilo SHARED ${SRC_FILES})
target_link_libraries(msilo PRIVATE common_modules kamailio)

target_link_libraries(msilo PRIVATE srdb1)
set_target_properties(
  msilo PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
