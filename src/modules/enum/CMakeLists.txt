cmake_minimum_required(VERSION 3.10)

project(enum)

file(GLOB SRC_FILES "*.c")

add_library(enum SHARED ${SRC_FILES})
target_link_libraries(enum PRIVATE kamailio)
