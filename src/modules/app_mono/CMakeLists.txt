cmake_minimum_required(VERSION 3.10)

project(app_mono)

file(GLOB SRC_FILES "*.c")

add_library(app_mono SHARED ${SRC_FILES})
target_link_libraries(app_mono PRIVATE common_modules kamailio)
