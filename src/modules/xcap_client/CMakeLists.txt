cmake_minimum_required(VERSION 3.10)

project(xcap_client)

file(GLOB SRC_FILES "*.c")

add_library(xcap_client SHARED ${SRC_FILES})
target_link_libraries(xcap_client PRIVATE common_modules kamailio)

target_link_libraries(xcap_client PRIVATE srdb1)
set_target_properties(
  xcap_client PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
