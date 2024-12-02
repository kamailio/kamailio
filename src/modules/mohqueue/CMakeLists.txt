cmake_minimum_required(VERSION 3.10)

project(mohqueue)

file(GLOB SRC_FILES "*.c")

add_library(mohqueue SHARED ${SRC_FILES})
target_link_libraries(mohqueue PRIVATE common_modules kamailio)

target_link_libraries(mohqueue PRIVATE srdb1)
