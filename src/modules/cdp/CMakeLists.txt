cmake_minimum_required(VERSION 3.10)

project(cdp)

file(GLOB SRC_FILES "*.c")

add_library(cdp SHARED ${SRC_FILES})
target_link_libraries(cdp PRIVATE common_modules kamailio)
