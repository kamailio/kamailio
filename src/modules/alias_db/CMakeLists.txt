cmake_minimum_required(VERSION 3.10)

project(alias_db)

file(GLOB SRC_FILES "*.c")

add_library(alias_db SHARED ${SRC_FILES})
target_link_libraries(alias_db PRIVATE common_modules kamailio)

target_link_libraries(alias_db PRIVATE srdb1)
set_target_properties(
  alias_db PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
