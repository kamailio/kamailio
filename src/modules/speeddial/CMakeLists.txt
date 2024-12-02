cmake_minimum_required(VERSION 3.10)

project(speeddial)

file(GLOB SRC_FILES "*.c")

add_library(speeddial SHARED ${SRC_FILES})
target_link_libraries(speeddial PRIVATE common_modules kamailio)

target_link_libraries(speeddial PRIVATE srdb1)
set_target_properties(
  speeddial PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
