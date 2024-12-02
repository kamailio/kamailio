cmake_minimum_required(VERSION 3.10)

project(cfgutils)

file(GLOB SRC_FILES "*.c")

add_library(cfgutils SHARED ${SRC_FILES})
target_link_libraries(cfgutils PRIVATE common_modules kamailio)
