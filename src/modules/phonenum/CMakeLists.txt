cmake_minimum_required(VERSION 3.10)

project(phonenum)

file(GLOB SRC_FILES "*.c")

add_library(phonenum SHARED ${SRC_FILES})
target_link_libraries(phonenum PRIVATE common_modules kamailio)
