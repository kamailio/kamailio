cmake_minimum_required(VERSION 3.10)

project(db_sqlite)

file(GLOB SRC_FILES "*.c")

add_library(db_sqlite SHARED ${SRC_FILES})
target_link_libraries(db_sqlite PRIVATE common_modules kamailio)

target_link_libraries(db_sqlite PRIVATE srdb1)
