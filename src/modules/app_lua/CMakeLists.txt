cmake_minimum_required(VERSION 3.10)

project(app_lua)

file(GLOB SRC_FILES "*.c")

add_library(app_lua SHARED ${SRC_FILES})
target_link_libraries(app_lua PRIVATE common_modules kamailio)
