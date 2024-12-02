cmake_minimum_required(VERSION 3.10)

project(secsipid_proc)

file(GLOB SRC_FILES "*.c")

add_library(secsipid_proc SHARED ${SRC_FILES})
target_link_libraries(secsipid_proc PRIVATE common_modules kamailio)
