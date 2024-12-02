cmake_minimum_required(VERSION 3.10)

project(osp)

file(GLOB SRC_FILES "*.c")

add_library(osp SHARED ${SRC_FILES})
target_link_libraries(osp PRIVATE common_modules kamailio)
