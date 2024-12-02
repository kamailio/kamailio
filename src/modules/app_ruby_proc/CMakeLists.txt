cmake_minimum_required(VERSION 3.10)

project(app_ruby_proc)

file(GLOB SRC_FILES "*.c")

add_library(app_ruby_proc SHARED ${SRC_FILES})
target_link_libraries(app_ruby_proc PRIVATE common_modules kamailio)
