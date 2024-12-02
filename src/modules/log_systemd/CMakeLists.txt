cmake_minimum_required(VERSION 3.10)

project(log_systemd)

file(GLOB SRC_FILES "*.c")

add_library(log_systemd SHARED ${SRC_FILES})
target_link_libraries(log_systemd PRIVATE common_modules kamailio)
