cmake_minimum_required(VERSION 3.10)

project(avpops)

file(GLOB SRC_FILES "*.c")

add_library(avpops SHARED ${SRC_FILES})
target_link_libraries(avpops PRIVATE common_modules kamailio)

target_link_libraries(avpops PRIVATE srdb1)
set_target_properties(
  avpops PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
