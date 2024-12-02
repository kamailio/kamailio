cmake_minimum_required(VERSION 3.10)

project(rtjson)

file(GLOB SRC_FILES "*.c")

add_library(rtjson SHARED ${SRC_FILES})
target_link_libraries(rtjson PRIVATE common_modules kamailio)
