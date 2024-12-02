cmake_minimum_required(VERSION 3.10)

project(jsonrpcc)

file(GLOB SRC_FILES "*.c")

add_library(jsonrpcc SHARED ${SRC_FILES})
target_link_libraries(jsonrpcc PRIVATE common_modules kamailio)
