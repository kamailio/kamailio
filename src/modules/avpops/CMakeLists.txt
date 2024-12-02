cmake_minimum_required(VERSION 3.10)

project(avpops)

file(GLOB SRC_FILES "*.c")

add_library(avpops SHARED ${SRC_FILES})
target_link_libraries(avpops PRIVATE common_modules kamailio)

target_link_libraries(avpops PRIVATE srdb1)
