cmake_minimum_required(VERSION 3.10)

project(xmlops)

file(GLOB SRC_FILES "*.c")

add_library(xmlops SHARED ${SRC_FILES})
target_link_libraries(xmlops PRIVATE common_modules kamailio)
