cmake_minimum_required(VERSION 3.10)

project(pua)

file(GLOB SRC_FILES "*.c")

add_library(pua SHARED ${SRC_FILES})
target_link_libraries(pua PRIVATE common_modules kamailio)

target_link_libraries(pua PRIVATE srdb1)
set_target_properties(
  pua PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
