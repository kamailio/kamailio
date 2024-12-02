cmake_minimum_required(VERSION 3.10)

project(crypto)

file(GLOB SRC_FILES "*.c")

add_library(crypto SHARED ${SRC_FILES})
target_link_libraries(crypto PRIVATE common_modules kamailio)
