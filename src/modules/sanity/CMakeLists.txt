cmake_minimum_required(VERSION 3.10)

project(sanity)

file(GLOB SRC_FILES "*.c")

add_library(sanity SHARED ${SRC_FILES})
target_link_libraries(sanity PRIVATE common common_modules kamailio)