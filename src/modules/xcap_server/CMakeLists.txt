cmake_minimum_required(VERSION 3.10)

project(xcap_server)

file(GLOB SRC_FILES "*.c")

add_library(xcap_server SHARED ${SRC_FILES})
target_link_libraries(xcap_server PRIVATE common_modules kamailio)

target_link_libraries(xcap_server PRIVATE srdb1)
set_target_properties(
  xcap_server PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
