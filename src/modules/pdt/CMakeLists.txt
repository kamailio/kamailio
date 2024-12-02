cmake_minimum_required(VERSION 3.10)

project(pdt)

file(GLOB SRC_FILES "*.c")

add_library(pdt SHARED ${SRC_FILES})

target_link_libraries(pdt PRIVATE kamailio)
target_link_libraries(pdt PRIVATE srdb1)
