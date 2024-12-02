cmake_minimum_required(VERSION 3.10)

project(secfilter)

file(GLOB SRC_FILES "*.c")

add_library(secfilter SHARED ${SRC_FILES})
target_link_libraries(secfilter PRIVATE common_modules kamailio)

target_link_libraries(secfilter PRIVATE srdb1)
set_target_properties(
  secfilter PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
