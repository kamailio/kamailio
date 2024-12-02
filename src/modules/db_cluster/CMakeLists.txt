cmake_minimum_required(VERSION 3.10)

project(db_cluster)

file(GLOB SRC_FILES "*.c")

add_library(db_cluster SHARED ${SRC_FILES})
target_link_libraries(db_cluster PRIVATE common_modules kamailio)

target_link_libraries(db_cluster PRIVATE srdb1)
set_target_properties(
  db_cluster PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
