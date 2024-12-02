cmake_minimum_required(VERSION 3.10)

project(janssonrpcc)

file(GLOB SRC_FILES "*.c")

add_library(janssonrpcc SHARED ${SRC_FILES})
target_link_libraries(janssonrpcc PRIVATE common_modules kamailio)
