cmake_minimum_required(VERSION 3.10)

project(posops)

file(GLOB SRC_FILES "*.c")

add_library(posops SHARED ${SRC_FILES})
target_link_libraries(posops PRIVATE kamailio)
