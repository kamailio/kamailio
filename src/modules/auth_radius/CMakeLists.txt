cmake_minimum_required(VERSION 3.10)

project(auth_radius)

file(GLOB SRC_FILES "*.c")

add_library(auth_radius SHARED ${SRC_FILES})
target_link_libraries(auth_radius PRIVATE common_modules kamailio)
