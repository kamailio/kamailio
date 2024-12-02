cmake_minimum_required(VERSION 3.10)

project(tlsa)

file(GLOB SRC_FILES "*.c")

add_library(tlsa SHARED ${SRC_FILES})
target_link_libraries(tlsa PRIVATE common_modules kamailio)
