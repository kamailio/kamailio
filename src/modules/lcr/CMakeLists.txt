cmake_minimum_required(VERSION 3.10)

project(lcr)

file(GLOB SRC_FILES "*.c")

add_library(lcr SHARED ${SRC_FILES})
target_link_libraries(lcr PRIVATE common_modules kamailio)

target_link_libraries(lcr PRIVATE srdb1)
