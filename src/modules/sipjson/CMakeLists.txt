cmake_minimum_required(VERSION 3.10)

project(sipjson)

file(GLOB SRC_FILES "*.c")

add_library(sipjson SHARED ${SRC_FILES})
target_link_libraries(sipjson PRIVATE kamailio)
