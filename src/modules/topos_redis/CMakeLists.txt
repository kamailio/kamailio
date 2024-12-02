cmake_minimum_required(VERSION 3.10)

project(topos_redis)

file(GLOB SRC_FILES "*.c")

add_library(topos_redis SHARED ${SRC_FILES})
target_link_libraries(topos_redis PRIVATE common_modules kamailio)
