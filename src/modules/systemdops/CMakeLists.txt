cmake_minimum_required(VERSION 3.10)

project(systemdops)

file(GLOB SRC_FILES "*.c")

add_library(systemdops SHARED ${SRC_FILES})
target_link_libraries(systemdops PRIVATE common_modules kamailio)
