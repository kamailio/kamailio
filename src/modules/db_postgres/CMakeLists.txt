cmake_minimum_required(VERSION 3.10)

project(db_postgres)

file(GLOB SRC_FILES "*.c")

add_library(db_postgres SHARED ${SRC_FILES})
target_link_libraries(db_postgres PRIVATE common_modules kamailio)

target_link_libraries(db_postgres PRIVATE srdb1)
