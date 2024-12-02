cmake_minimum_required(VERSION 3.10)

project(presence_profile)

file(GLOB SRC_FILES "*.c")

add_library(presence_profile SHARED ${SRC_FILES})
target_link_libraries(presence_profile PRIVATE common_modules kamailio)
