cmake_minimum_required(VERSION 3.10)

project(sctp)

file(GLOB SRC_FILES "*.c")

add_library(sctp SHARED ${SRC_FILES})
target_link_libraries(sctp PRIVATE common_modules kamailio)
