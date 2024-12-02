cmake_minimum_required(VERSION 3.10)

project(textopsx)

file(GLOB SRC_FILES "*.c")

add_library(textopsx SHARED ${SRC_FILES})
target_link_libraries(textopsx PRIVATE kamailio)
