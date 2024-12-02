cmake_minimum_required(VERSION 3.10)

project(nathelper)

file(GLOB SRC_FILES "*.c")

add_library(nathelper SHARED ${SRC_FILES})
target_link_libraries(nathelper PRIVATE common_modules kamailio)
