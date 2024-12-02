cmake_minimum_required(VERSION 3.10)

project(msrp)

file(GLOB SRC_FILES "*.c")

add_library(msrp SHARED ${SRC_FILES})
target_link_libraries(msrp PRIVATE kamailio)
