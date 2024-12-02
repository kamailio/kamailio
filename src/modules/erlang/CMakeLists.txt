cmake_minimum_required(VERSION 3.10)

project(erlang)

file(GLOB SRC_FILES "*.c")

add_library(erlang SHARED ${SRC_FILES})
target_link_libraries(erlang PRIVATE common_modules kamailio)
