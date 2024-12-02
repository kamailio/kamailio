cmake_minimum_required(VERSION 3.10)

project(app_python3)

file(GLOB SRC_FILES "*.c")

add_library(app_python3 SHARED ${SRC_FILES})
target_link_libraries(app_python3 PRIVATE common_modules kamailio)
