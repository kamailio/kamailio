cmake_minimum_required(VERSION 3.10)

project(drouting)

file(GLOB SRC_FILES "*.c")

add_library(drouting SHARED ${SRC_FILES})
target_link_libraries(drouting PRIVATE common_modules kamailio)

target_link_libraries(drouting PRIVATE srdb1)
