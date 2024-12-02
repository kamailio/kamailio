cmake_minimum_required(VERSION 3.10)

project(ss7ops)

file(GLOB SRC_FILES "*.c")

add_library(ss7ops SHARED ${SRC_FILES})
target_link_libraries(ss7ops PRIVATE kamailio)
