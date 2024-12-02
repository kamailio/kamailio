cmake_minimum_required(VERSION 3.10)

project(cfg_rpc)

file(GLOB SRC_FILES "*.c")

add_library(cfg_rpc SHARED ${SRC_FILES})
target_link_libraries(cfg_rpc PRIVATE kamailio)

# TODO: Also install kamcmd binary
