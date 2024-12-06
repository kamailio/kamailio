cmake_minimum_required(VERSION 3.10)

project(db_text)

file(GLOB SRC_FILES "*.c")

add_library(db_text SHARED ${SRC_FILES})
target_link_libraries(db_text PRIVATE kamailio)
