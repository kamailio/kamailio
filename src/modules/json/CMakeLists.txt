cmake_minimum_required(VERSION 3.10)

project(json)

file(GLOB SRC_FILES "*.c")

add_library(json SHARED ${SRC_FILES})
target_link_libraries(json PRIVATE common_modules kamailio)
