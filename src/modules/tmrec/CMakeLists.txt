cmake_minimum_required(VERSION 3.10)

project(tmrec)

file(GLOB SRC_FILES "*.c")

add_library(tmrec SHARED ${SRC_FILES})
target_link_libraries(tmrec PRIVATE common_modules kamailio)
