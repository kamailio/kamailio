cmake_minimum_required(VERSION 3.10)

project(usrloc)

file(GLOB SRC_FILES "*.c")

add_library(usrloc SHARED ${SRC_FILES})

target_link_libraries(usrloc PRIVATE kamailio)
target_link_libraries(usrloc PRIVATE srdb1)
