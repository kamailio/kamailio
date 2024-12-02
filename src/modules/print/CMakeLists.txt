cmake_minimum_required(VERSION 3.10)

project(print)

file(GLOB SRC_FILES "*.c")

add_library(print SHARED ${SRC_FILES})
target_link_libraries(print PRIVATE common_modules kamailio)
