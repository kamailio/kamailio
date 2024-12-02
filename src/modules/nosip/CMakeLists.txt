cmake_minimum_required(VERSION 3.10)

project(nosip)

file(GLOB SRC_FILES "*.c")

add_library(nosip SHARED ${SRC_FILES})
target_link_libraries(nosip PRIVATE kamailio)
