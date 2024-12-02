cmake_minimum_required(VERSION 3.10)

project(acc_json)

file(GLOB SRC_FILES "*.c")

add_library(acc_json SHARED ${SRC_FILES})
target_link_libraries(acc_json PRIVATE common_modules kamailio)
