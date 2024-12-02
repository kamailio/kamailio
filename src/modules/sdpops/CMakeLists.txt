cmake_minimum_required(VERSION 3.10)

project(sdpops)

file(GLOB SRC_FILES "*.c")

add_library(sdpops SHARED ${SRC_FILES})
target_link_libraries(sdpops PRIVATE kamailio)
