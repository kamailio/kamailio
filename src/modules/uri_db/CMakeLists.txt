cmake_minimum_required(VERSION 3.10)

project(uri_db)

file(GLOB SRC_FILES "*.c")

add_library(uri_db SHARED ${SRC_FILES})

target_link_libraries(uri_db PRIVATE kamailio)
target_link_libraries(uri_db PRIVATE srdb1)
