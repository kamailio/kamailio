cmake_minimum_required(VERSION 3.10)

project(rtpengine)

file(GLOB SRC_FILES "*.c")

add_library(rtpengine SHARED ${SRC_FILES})
target_link_libraries(rtpengine PRIVATE common_modules kamailio)

target_link_libraries(rtpengine PRIVATE srdb1)
