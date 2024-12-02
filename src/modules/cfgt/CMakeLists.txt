cmake_minimum_required(VERSION 3.10)

project(cfgt)

file(GLOB SRC_FILES "*.c")

add_library(cfgt SHARED ${SRC_FILES})
target_link_libraries(cfgt PRIVATE common_modules kamailio)
