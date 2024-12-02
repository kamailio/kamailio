cmake_minimum_required(VERSION 3.10)

project(kafka)

file(GLOB SRC_FILES "*.c")

add_library(kafka SHARED ${SRC_FILES})
target_link_libraries(kafka PRIVATE common_modules kamailio)
