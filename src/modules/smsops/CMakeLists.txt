cmake_minimum_required(VERSION 3.10)

project(smsops)

file(GLOB SRC_FILES "*.c")

add_library(smsops SHARED ${SRC_FILES})
target_link_libraries(smsops PRIVATE common_modules kamailio)
