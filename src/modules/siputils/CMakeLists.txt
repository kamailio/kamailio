cmake_minimum_required(VERSION 3.10)

project(siputils)

file(GLOB SRC_FILES "*.c")

add_library(siputils SHARED ${SRC_FILES})
target_link_libraries(siputils PRIVATE common_modules kamailio)

target_link_libraries(siputils PRIVATE srdb1)
set_target_properties(
  siputils PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
