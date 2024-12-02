cmake_minimum_required(VERSION 3.10)

project(sipt)

file(GLOB SRC_FILES "*.c")

add_library(sipt SHARED ${SRC_FILES})
target_link_libraries(sipt PRIVATE common_modules kamailio)
