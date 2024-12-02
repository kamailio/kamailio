cmake_minimum_required(VERSION 3.10)

project(pua_dialoginfo)

file(GLOB SRC_FILES "*.c")

add_library(pua_dialoginfo SHARED ${SRC_FILES})
target_link_libraries(pua_dialoginfo PRIVATE common_modules kamailio)
