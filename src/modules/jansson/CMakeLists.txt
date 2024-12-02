cmake_minimum_required(VERSION 3.10)

project(jansson)

file(GLOB SRC_FILES "*.c")

add_library(jansson SHARED ${SRC_FILES})
target_link_libraries(jansson PRIVATE common_modules kamailio)
