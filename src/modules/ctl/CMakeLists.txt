cmake_minimum_required(VERSION 3.10)

project(ctl)

file(GLOB SRC_FILES "*.c")

add_library(ctl SHARED ${SRC_FILES})
target_compile_definitions(ctl PRIVATE USE_FIFO CTL_SYSTEM_MALLOC)
target_link_libraries(ctl PRIVATE common_modules kamailio)

# TODO: Also install kamcmd binary
