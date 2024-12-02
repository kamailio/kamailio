cmake_minimum_required(VERSION 3.10)

project(dialog)

file(GLOB SRC_FILES "*.c")

add_library(dialog SHARED ${SRC_FILES})

target_link_libraries(dialog PRIVATE kamailio)
target_link_libraries(dialog PRIVATE srdb1)
