cmake_minimum_required(VERSION 3.10)

project(uri_db)

file(GLOB SRC_FILES "*.c")

add_library(uri_db SHARED ${SRC_FILES})
target_link_libraries(uri_db PRIVATE common_modules kamailio)

target_link_libraries(uri_db PRIVATE srdb1)
set_target_properties(
  uri_db PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
