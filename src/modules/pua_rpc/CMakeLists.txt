cmake_minimum_required(VERSION 3.10)

project(pua_rpc)

file(GLOB SRC_FILES "*.c")

add_library(pua_rpc SHARED ${SRC_FILES})
target_link_libraries(pua_rpc PRIVATE common_modules kamailio)
