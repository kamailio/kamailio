cmake_minimum_required(VERSION 3.10)

project(dialplan)

file(GLOB SRC_FILES "*.c")

add_library(dialplan SHARED ${SRC_FILES})
target_link_libraries(dialplan PRIVATE common_modules kamailio)

target_link_libraries(dialplan PRIVATE srdb1)
set_target_properties(
  dialplan PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
