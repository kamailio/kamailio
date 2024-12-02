cmake_minimum_required(VERSION 3.10)

project(kemix)

file(GLOB SRC_FILES "*.c")

add_library(kemix SHARED ${SRC_FILES})
target_link_libraries(kemix PRIVATE kamailio)
