cmake_minimum_required(VERSION 3.10)

project(app_ruby)

file(GLOB SRC_FILES "*.c")

add_library(app_ruby SHARED ${SRC_FILES})
target_link_libraries(app_ruby PRIVATE common_modules kamailio)
