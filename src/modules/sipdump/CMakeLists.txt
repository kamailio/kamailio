cmake_minimum_required(VERSION 3.10)

project(sipdump)

file(GLOB SRC_FILES "*.c")

add_library(sipdump SHARED ${SRC_FILES})
target_link_libraries(sipdump PRIVATE common_modules kamailio)
