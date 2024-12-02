cmake_minimum_required(VERSION 3.10)

project(pua_bla)

file(GLOB SRC_FILES "*.c")

add_library(pua_bla SHARED ${SRC_FILES})
target_link_libraries(pua_bla PRIVATE common_modules kamailio)
