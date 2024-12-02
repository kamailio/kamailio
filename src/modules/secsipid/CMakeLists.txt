cmake_minimum_required(VERSION 3.10)

project(secsipid)

file(GLOB SRC_FILES "*.c")

add_library(secsipid SHARED ${SRC_FILES})
target_link_libraries(secsipid PRIVATE common_modules kamailio)
