cmake_minimum_required(VERSION 3.10)

project(rtimer)

file(GLOB SRC_FILES "*.c")

add_library(rtimer SHARED ${SRC_FILES})
target_link_libraries(rtimer PRIVATE common_modules kamailio)
