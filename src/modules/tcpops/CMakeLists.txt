cmake_minimum_required(VERSION 3.10)

project(tcpops)

file(GLOB SRC_FILES "*.c")

add_library(tcpops SHARED ${SRC_FILES})
target_link_libraries(tcpops PRIVATE common_modules kamailio)
