cmake_minimum_required(VERSION 3.10)

project(sl)

file(GLOB SRC_FILES "*.c")

add_library(sl SHARED ${SRC_FILES})
target_link_libraries(sl PRIVATE common_modules kamailio)
