cmake_minimum_required(VERSION 3.10)

project(pua_json)

file(GLOB SRC_FILES "*.c")

add_library(pua_json SHARED ${SRC_FILES})
target_link_libraries(pua_json PRIVATE common_modules kamailio)
