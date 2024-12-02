cmake_minimum_required(VERSION 3.10)

project(db_unixodbc)

file(GLOB SRC_FILES "*.c")

add_library(db_unixodbc SHARED ${SRC_FILES})
target_link_libraries(db_unixodbc PRIVATE common_modules kamailio)

target_link_libraries(db_unixodbc PRIVATE srdb1)
