cmake_minimum_required(VERSION 3.10)

project(dispatcher)

file(GLOB SRC_FILES "*.c")

add_library(dispatcher SHARED ${SRC_FILES})

target_link_libraries(dispatcher PRIVATE kamailio)
target_link_libraries(dispatcher PRIVATE srdb1)
