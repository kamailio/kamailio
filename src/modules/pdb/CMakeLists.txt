cmake_minimum_required(VERSION 3.10)

project(pdb)

file(GLOB SRC_FILES "*.c")

add_library(pdb SHARED ${SRC_FILES})
target_link_libraries(pdb PRIVATE common_modules kamailio)
