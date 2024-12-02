cmake_minimum_required(VERSION 3.10)

project(topoh)

file(GLOB SRC_FILES "*.c")

add_library(topoh SHARED ${SRC_FILES})
target_link_libraries(topoh PRIVATE common_modules kamailio)
