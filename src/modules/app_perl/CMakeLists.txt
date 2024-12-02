cmake_minimum_required(VERSION 3.10)

project(app_perl)

file(GLOB SRC_FILES "*.c")

add_library(app_perl SHARED ${SRC_FILES})
target_link_libraries(app_perl PRIVATE common_modules kamailio)
