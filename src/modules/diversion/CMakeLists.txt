cmake_minimum_required(VERSION 3.10)

project(diversion)

file(GLOB SRC_FILES "*.c")

add_library(diversion SHARED ${SRC_FILES})
target_link_libraries(diversion PRIVATE kamailio)
