cmake_minimum_required(VERSION 3.10)

project(db2_ops)

file(GLOB SRC_FILES "*.c")

add_library(db2_ops SHARED ${SRC_FILES})

target_link_libraries(db2_ops PRIVATE kamailio)
