cmake_minimum_required(VERSION 3.10)

project(cplc)

file(GLOB SRC_FILES "*.c")

add_library(cplc SHARED ${SRC_FILES})
target_link_libraries(cplc PRIVATE common_modules kamailio)

target_link_libraries(cplc PRIVATE srdb1)
set_target_properties(
  cplc PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
