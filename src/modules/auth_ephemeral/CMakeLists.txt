cmake_minimum_required(VERSION 3.10)

project(auth_ephemeral)

file(GLOB SRC_FILES "*.c")

add_library(auth_ephemeral SHARED ${SRC_FILES})
target_link_libraries(auth_ephemeral PRIVATE kamailio)
