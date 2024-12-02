cmake_minimum_required(VERSION 3.10)

project(acc_radius)

file(GLOB SRC_FILES "*.c")

add_library(acc_radius SHARED ${SRC_FILES})
target_link_libraries(acc_radius PRIVATE common_modules kamailio)
