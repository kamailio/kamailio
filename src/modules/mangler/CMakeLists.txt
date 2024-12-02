cmake_minimum_required(VERSION 3.10)

project(mangler)

file(GLOB SRC_FILES "*.c")

add_library(mangler SHARED ${SRC_FILES})
target_link_libraries(mangler PRIVATE kamailio)
