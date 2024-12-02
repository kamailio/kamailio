cmake_minimum_required(VERSION 3.10)

project(db_cassandra)

file(GLOB SRC_FILES "*.c")

add_library(db_cassandra SHARED ${SRC_FILES})
target_link_libraries(db_cassandra PRIVATE common_modules kamailio)

target_link_libraries(db_cassandra PRIVATE srdb1)
set_target_properties(
  db_cassandra PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
