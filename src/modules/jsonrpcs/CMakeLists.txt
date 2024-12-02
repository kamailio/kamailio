cmake_minimum_required(VERSION 3.10)

project(jsonrpcs)

file(GLOB jsonrpcs_SRC "*.c")

add_library(jsonrpcs SHARED ${jsonrpcs_SRC})
target_link_libraries(jsonrpcs PRIVATE kamailio)
