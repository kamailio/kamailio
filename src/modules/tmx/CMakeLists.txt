cmake_minimum_required(VERSION 3.10)

project(tmx)

file(GLOB SRC_FILES "*.c")

add_library(tmx SHARED ${SRC_FILES})
target_link_libraries(tmx PRIVATE common common_modules kamailio)
# target_compile_definitions(tmx PRIVATE STATISTICS)