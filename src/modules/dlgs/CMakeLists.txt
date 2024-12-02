cmake_minimum_required(VERSION 3.10)

project(dlgs)

file(GLOB SRC_FILES "*.c")

add_library(dlgs SHARED ${SRC_FILES})
target_link_libraries(dlgs PRIVATE common_modules kamailio)
