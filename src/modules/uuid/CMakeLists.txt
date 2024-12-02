cmake_minimum_required(VERSION 3.10)

project(uuid)

file(GLOB SRC_FILES "*.c")

add_library(uuid SHARED ${SRC_FILES})
target_link_libraries(uuid PRIVATE common_modules kamailio)
