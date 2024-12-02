cmake_minimum_required(VERSION 3.10)

project(presence_mwi)

file(GLOB SRC_FILES "*.c")

add_library(presence_mwi SHARED ${SRC_FILES})
target_link_libraries(presence_mwi PRIVATE kamailio)
