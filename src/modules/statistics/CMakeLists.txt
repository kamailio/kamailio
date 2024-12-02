cmake_minimum_required(VERSION 3.10)

project(statistics)

file(GLOB SRC_FILES "*.c")

add_library(statistics SHARED ${SRC_FILES})
target_link_libraries(statistics PRIVATE common_modules kamailio)
