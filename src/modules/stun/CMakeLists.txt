cmake_minimum_required(VERSION 3.10)

project(stun)

file(GLOB SRC_FILES "*.c")

add_library(stun SHARED ${SRC_FILES})
target_link_libraries(stun PRIVATE common_modules kamailio)
