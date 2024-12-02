cmake_minimum_required(VERSION 3.10)

project(lwsc)

file(GLOB SRC_FILES "*.c")

add_library(lwsc SHARED ${SRC_FILES})
target_link_libraries(lwsc PRIVATE common_modules kamailio)
