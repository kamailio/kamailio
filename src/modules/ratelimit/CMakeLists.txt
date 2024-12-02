cmake_minimum_required(VERSION 3.10)

project(ratelimit)

file(GLOB SRC_FILES "*.c")

add_library(ratelimit SHARED ${SRC_FILES})
target_link_libraries(ratelimit PRIVATE common_modules kamailio)
