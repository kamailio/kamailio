cmake_minimum_required(VERSION 3.10)

project(sca)

file(GLOB SRC_FILES "*.c")

add_library(sca SHARED ${SRC_FILES})

target_link_libraries(sca PRIVATE kamailio)
target_link_libraries(sca PRIVATE srdb1)
