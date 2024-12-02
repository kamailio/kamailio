cmake_minimum_required(VERSION 3.10)

project(dmq_usrloc)

file(GLOB SRC_FILES "*.c")

add_library(dmq_usrloc SHARED ${SRC_FILES})
target_link_libraries(dmq_usrloc PRIVATE common_modules kamailio)
