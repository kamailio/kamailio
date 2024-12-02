cmake_minimum_required(VERSION 3.10)

project(nghttp2)

file(GLOB SRC_FILES "*.c")

add_library(nghttp2 SHARED ${SRC_FILES})
target_link_libraries(nghttp2 PRIVATE common_modules kamailio)
