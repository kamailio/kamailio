cmake_minimum_required(VERSION 3.10)

project(xlog)

file(GLOB SRC_FILES "*.c")

add_library(xlog SHARED ${SRC_FILES})
target_link_libraries(xlog PRIVATE common common_modules kamailio)