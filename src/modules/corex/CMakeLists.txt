cmake_minimum_required(VERSION 3.10)

project(corex)

file(GLOB SRC_FILES "*.c")

add_library(corex SHARED ${SRC_FILES})
target_link_libraries(corex PRIVATE common_modules kamailio)
