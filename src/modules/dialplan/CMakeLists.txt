cmake_minimum_required(VERSION 3.10)

project(dialplan)

file(GLOB SRC_FILES "*.c")

add_library(dialplan SHARED ${SRC_FILES})

target_link_libraries(dialplan PRIVATE kamailio)
