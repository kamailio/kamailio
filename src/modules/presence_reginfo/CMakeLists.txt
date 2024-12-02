cmake_minimum_required(VERSION 3.10)

project(presence_reginfo)

file(GLOB SRC_FILES "*.c")

add_library(presence_reginfo SHARED ${SRC_FILES})
target_link_libraries(presence_reginfo PRIVATE common_modules kamailio)
