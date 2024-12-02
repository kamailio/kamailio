cmake_minimum_required(VERSION 3.10)

project(peering)

file(GLOB SRC_FILES "*.c")

add_library(peering SHARED ${SRC_FILES})
target_link_libraries(peering PRIVATE common_modules kamailio)
