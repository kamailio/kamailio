cmake_minimum_required(VERSION 3.10)

project(ruxc)

file(GLOB SRC_FILES "*.c")

add_library(ruxc SHARED ${SRC_FILES})
target_link_libraries(ruxc PRIVATE common_modules kamailio)
