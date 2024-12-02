cmake_minimum_required(VERSION 3.10)

project(benchmark)

file(GLOB SRC_FILES "*.c")

add_library(benchmark SHARED ${SRC_FILES})
target_link_libraries(benchmark PRIVATE common_modules kamailio)
