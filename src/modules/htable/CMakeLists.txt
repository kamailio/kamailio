cmake_minimum_required(VERSION 3.10)

project(htable)

file(GLOB SRC_FILES "*.c")

add_library(htable SHARED ${SRC_FILES})
target_link_libraries(htable PRIVATE common common_modules kamailio)

# TODO: link with srdb1