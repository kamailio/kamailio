cmake_minimum_required(VERSION 3.10)

project(mqueue)

file(GLOB SRC_FILES "*.c")

add_library(mqueue SHARED ${SRC_FILES})
target_link_libraries(mqueue PRIVATE common_modules kamailio)

target_link_libraries(mqueue PRIVATE srdb1)
set_target_properties(
  mqueue PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
