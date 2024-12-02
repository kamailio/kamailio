cmake_minimum_required(VERSION 3.10)

project(auth_diameter)

file(GLOB SRC_FILES "*.c")

add_library(auth_diameter SHARED ${SRC_FILES})
target_link_libraries(auth_diameter PRIVATE common_modules kamailio)
