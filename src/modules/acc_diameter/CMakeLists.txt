cmake_minimum_required(VERSION 3.10)

project(acc_diameter)

file(GLOB SRC_FILES "*.c")

add_library(acc_diameter SHARED ${SRC_FILES})
target_link_libraries(acc_diameter PRIVATE common_modules kamailio)
