cmake_minimum_required(VERSION 3.10)

project(presence_dialoginfo)

file(GLOB SRC_FILES "*.c")

add_library(presence_dialoginfo SHARED ${SRC_FILES})
target_link_libraries(presence_dialoginfo PRIVATE common_modules kamailio)
