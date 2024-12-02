cmake_minimum_required(VERSION 3.10)

project(sst)

file(GLOB SRC_FILES "*.c")

add_library(sst SHARED ${SRC_FILES})
target_link_libraries(sst PRIVATE kamailio)
