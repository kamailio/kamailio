cmake_minimum_required(VERSION 3.10)

project(db_flatstore)

file(GLOB SRC_FILES "*.c")

add_library(db_flatstore SHARED ${SRC_FILES})

target_link_libraries(db_flatstore PRIVATE kamailio)
