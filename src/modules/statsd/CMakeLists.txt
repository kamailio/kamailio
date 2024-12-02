cmake_minimum_required(VERSION 3.10)

project(statsd)

file(GLOB SRC_FILES "*.c")

add_library(statsd SHARED ${SRC_FILES})
target_link_libraries(statsd PRIVATE kamailio)
