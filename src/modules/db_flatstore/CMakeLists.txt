cmake_minimum_required(VERSION 3.10)

project(db_flatstore)

file(GLOB SRC_FILES "*.c")

add_library(db_flatstore SHARED ${SRC_FILES})
target_link_libraries(db_flatstore PRIVATE common_modules kamailio)

target_link_libraries(db_flatstore PRIVATE srdb1)
set_target_properties(
  db_flatstore PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
