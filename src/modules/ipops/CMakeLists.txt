cmake_minimum_required(VERSION 3.10)

project(ipops)

file(GLOB SRC_FILES "*.c")

add_library(ipops SHARED ${SRC_FILES})
target_link_libraries(ipops PRIVATE kamailio)
