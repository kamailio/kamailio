cmake_minimum_required(VERSION 3.10)

project(gcrypt)

file(GLOB SRC_FILES "*.c")

add_library(gcrypt SHARED ${SRC_FILES})
target_link_libraries(gcrypt PRIVATE common_modules kamailio)
