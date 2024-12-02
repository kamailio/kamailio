cmake_minimum_required(VERSION 3.10)

project(avp)

file(GLOB SRC_FILES "*.c")

add_library(avp SHARED ${SRC_FILES})
target_link_libraries(avp PRIVATE kamailio)
