cmake_minimum_required(VERSION 3.10)

project(uid_gflags)

file(GLOB SRC_FILES "*.c")

add_library(uid_gflags SHARED ${SRC_FILES})
target_link_libraries(uid_gflags PRIVATE common_modules kamailio)
