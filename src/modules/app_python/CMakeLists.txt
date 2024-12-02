cmake_minimum_required(VERSION 3.10)

project(app_python)

file(GLOB SRC_FILES "*.c")

add_library(app_python SHARED ${SRC_FILES})
target_link_libraries(app_python PRIVATE common_modules kamailio)
