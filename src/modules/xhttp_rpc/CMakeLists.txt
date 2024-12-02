cmake_minimum_required(VERSION 3.10)

project(xhttp_rpc)

file(GLOB SRC_FILES "*.c")

add_library(xhttp_rpc SHARED ${SRC_FILES})
target_link_libraries(xhttp_rpc PRIVATE common_modules kamailio)
