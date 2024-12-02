cmake_minimum_required(VERSION 3.10)

project(pv)

file(GLOB SRC_FILES "*.c")

add_library(pv SHARED ${SRC_FILES})
target_link_libraries(pv PRIVATE common_modules kamailio)
# target_compile_definitions(pv PRIVATE STATISTICS)
