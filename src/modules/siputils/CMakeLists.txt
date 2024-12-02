cmake_minimum_required(VERSION 3.10)

project(siputils)

file(GLOB SRC_FILES "*.c")

add_library(siputils SHARED ${SRC_FILES})

target_link_libraries(siputils PRIVATE kamailio)
target_link_libraries(siputils PRIVATE srdb1)
