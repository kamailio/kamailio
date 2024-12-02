cmake_minimum_required(VERSION 3.10)

project(pua_usrloc)

file(GLOB SRC_FILES "*.c")

add_library(pua_usrloc SHARED ${SRC_FILES})
target_link_libraries(pua_usrloc PRIVATE common_modules kamailio)
