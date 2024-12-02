cmake_minimum_required(VERSION 3.10)

project(pipelimit)

file(GLOB SRC_FILES "*.c")

add_library(pipelimit SHARED ${SRC_FILES})
target_link_libraries(pipelimit PRIVATE common_modules kamailio)

target_link_libraries(pipelimit PRIVATE srdb1)
set_target_properties(
  pipelimit PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
