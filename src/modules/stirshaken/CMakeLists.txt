cmake_minimum_required(VERSION 3.10)

project(stirshaken)

file(GLOB SRC_FILES "*.c")

add_library(stirshaken SHARED ${SRC_FILES})
target_link_libraries(stirshaken PRIVATE common_modules kamailio)
