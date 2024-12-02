cmake_minimum_required(VERSION 3.10)

project(db_text)

file(GLOB SRC_FILES "*.c")

add_library(db_text SHARED ${SRC_FILES})
target_link_libraries(db_text PRIVATE common_modules kamailio)

target_link_libraries(db_text PRIVATE srdb1)
set_target_properties(
  db_text PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
