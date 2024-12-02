cmake_minimum_required(VERSION 3.10)

project(seas)

file(GLOB SRC_FILES "*.c")

add_library(seas SHARED ${SRC_FILES})
target_link_libraries(seas PRIVATE common_modules kamailio)
