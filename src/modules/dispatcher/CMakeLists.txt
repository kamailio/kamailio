cmake_minimum_required(VERSION 3.10)

project(dispatcher)

file(GLOB SRC_FILES "*.c")

add_library(dispatcher SHARED ${SRC_FILES})
target_link_libraries(dispatcher PRIVATE common_modules kamailio)

target_link_libraries(dispatcher PRIVATE srdb1)
set_target_properties(
  dispatcher PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
