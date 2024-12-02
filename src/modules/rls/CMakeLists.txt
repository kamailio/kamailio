cmake_minimum_required(VERSION 3.10)

project(rls)

file(GLOB SRC_FILES "*.c")

add_library(rls SHARED ${SRC_FILES})
target_link_libraries(rls PRIVATE common_modules kamailio)

target_link_libraries(rls PRIVATE srdb1)
set_target_properties(
  rls PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
