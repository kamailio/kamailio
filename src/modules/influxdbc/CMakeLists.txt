cmake_minimum_required(VERSION 3.10)

project(influxdbc)

file(GLOB SRC_FILES "*.c")

add_library(influxdbc SHARED ${SRC_FILES})
target_link_libraries(influxdbc PRIVATE common_modules kamailio)
