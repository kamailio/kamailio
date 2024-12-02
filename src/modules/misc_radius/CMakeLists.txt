cmake_minimum_required(VERSION 3.10)

project(misc_radius)

file(GLOB SRC_FILES "*.c")

add_library(misc_radius SHARED ${SRC_FILES})
target_link_libraries(misc_radius PRIVATE common_modules kamailio)
