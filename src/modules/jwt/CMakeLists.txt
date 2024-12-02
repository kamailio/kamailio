cmake_minimum_required(VERSION 3.10)

project(jwt)

file(GLOB SRC_FILES "*.c")

add_library(jwt SHARED ${SRC_FILES})
target_link_libraries(jwt PRIVATE common_modules kamailio)
