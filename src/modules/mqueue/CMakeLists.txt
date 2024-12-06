cmake_minimum_required(VERSION 3.10)

project(mqueue)

file(GLOB SRC_FILES "*.c")

add_library(mqueue SHARED ${SRC_FILES})

target_link_libraries(mqueue PRIVATE kamailio)
