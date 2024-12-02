cmake_minimum_required(VERSION 3.10)

project(app_jsdt)

file(GLOB SRC_FILES "*.c")

add_library(app_jsdt SHARED ${SRC_FILES})
target_link_libraries(app_jsdt PRIVATE kamailio)
