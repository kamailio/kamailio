cmake_minimum_required(VERSION 3.10)

project(app_java)

file(GLOB SRC_FILES "*.c")

add_library(app_java SHARED ${SRC_FILES})
target_link_libraries(app_java PRIVATE common_modules kamailio)
