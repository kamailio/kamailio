cmake_minimum_required(VERSION 3.10)

project(math)

file(GLOB SRC_FILES "*.c")

add_library(math SHARED ${SRC_FILES})
target_link_libraries(math PRIVATE kamailio)
