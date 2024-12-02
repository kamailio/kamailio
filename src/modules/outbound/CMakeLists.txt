cmake_minimum_required(VERSION 3.10)

project(outbound)

file(GLOB SRC_FILES "*.c")

add_library(outbound SHARED ${SRC_FILES})
target_link_libraries(outbound PRIVATE common_modules kamailio)
