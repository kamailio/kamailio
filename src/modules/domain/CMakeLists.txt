cmake_minimum_required(VERSION 3.10)

project(domain)

file(GLOB SRC_FILES "*.c")

add_library(domain SHARED ${SRC_FILES})
target_link_libraries(domain PRIVATE common_modules kamailio)

target_link_libraries(domain PRIVATE srdb1)
set_target_properties(
  domain PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
