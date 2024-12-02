cmake_minimum_required(VERSION 3.10)

project(nsq)

file(GLOB SRC_FILES "*.c")

add_library(nsq SHARED ${SRC_FILES})
target_link_libraries(nsq PRIVATE common_modules kamailio)
