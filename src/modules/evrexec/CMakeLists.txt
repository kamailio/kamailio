cmake_minimum_required(VERSION 3.10)

project(evrexec)

file(GLOB SRC_FILES "*.c")

add_library(evrexec SHARED ${SRC_FILES})
target_link_libraries(evrexec PRIVATE common_modules kamailio)
