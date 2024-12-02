cmake_minimum_required(VERSION 3.10)

project(siprepo)

file(GLOB SRC_FILES "*.c")

add_library(siprepo SHARED ${SRC_FILES})
target_link_libraries(siprepo PRIVATE common_modules kamailio)
