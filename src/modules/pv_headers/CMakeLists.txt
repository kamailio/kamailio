cmake_minimum_required(VERSION 3.10)

project(pv_headers)

file(GLOB SRC_FILES "*.c")

add_library(pv_headers SHARED ${SRC_FILES})
target_link_libraries(pv_headers PRIVATE common_modules kamailio)
