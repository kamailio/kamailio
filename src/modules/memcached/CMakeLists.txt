cmake_minimum_required(VERSION 3.10)

project(memcached)

file(GLOB SRC_FILES "*.c")

add_library(memcached SHARED ${SRC_FILES})
target_link_libraries(memcached PRIVATE common_modules kamailio)
