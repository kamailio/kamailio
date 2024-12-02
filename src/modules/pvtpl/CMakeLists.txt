cmake_minimum_required(VERSION 3.10)

project(pvtpl)

file(GLOB SRC_FILES "*.c")

add_library(pvtpl SHARED ${SRC_FILES})
target_link_libraries(pvtpl PRIVATE common_modules kamailio)
