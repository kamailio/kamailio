cmake_minimum_required(VERSION 3.10)

project(presence_conference)

file(GLOB SRC_FILES "*.c")

add_library(presence_conference SHARED ${SRC_FILES})
target_link_libraries(presence_conference PRIVATE common_modules kamailio)
