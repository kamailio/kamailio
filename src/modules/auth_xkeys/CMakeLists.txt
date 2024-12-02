cmake_minimum_required(VERSION 3.10)

project(auth_xkeys)

file(GLOB SRC_FILES "*.c")

add_library(auth_xkeys SHARED ${SRC_FILES})
target_link_libraries(auth_xkeys PRIVATE common_modules kamailio)
