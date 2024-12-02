cmake_minimum_required(VERSION 3.10)

project(pike)

file(GLOB SRC_FILES "*.c")

add_library(pike SHARED ${SRC_FILES})
target_link_libraries(pike PRIVATE common_modules kamailio)
