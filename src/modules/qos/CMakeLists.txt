cmake_minimum_required(VERSION 3.10)

project(qos)

file(GLOB SRC_FILES "*.c")

add_library(qos SHARED ${SRC_FILES})
target_link_libraries(qos PRIVATE common_modules kamailio)
