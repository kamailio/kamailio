cmake_minimum_required(VERSION 3.10)

project(nat_traversal)

file(GLOB SRC_FILES "*.c")

add_library(nat_traversal SHARED ${SRC_FILES})
target_link_libraries(nat_traversal PRIVATE kamailio)
