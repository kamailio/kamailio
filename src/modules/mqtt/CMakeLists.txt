cmake_minimum_required(VERSION 3.10)

project(mqtt)

file(GLOB SRC_FILES "*.c")

add_library(mqtt SHARED ${SRC_FILES})
target_link_libraries(mqtt PRIVATE common_modules kamailio)
