cmake_minimum_required(VERSION 3.10)

project(textops)

file(GLOB SRC_FILES "*.c")

add_library(textops SHARED ${SRC_FILES})
target_link_libraries(textops PRIVATE kamailio)
