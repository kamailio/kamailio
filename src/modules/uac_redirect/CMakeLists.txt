cmake_minimum_required(VERSION 3.10)

project(uac_redirect)

file(GLOB SRC_FILES "*.c")

add_library(uac_redirect SHARED ${SRC_FILES})
target_link_libraries(uac_redirect PRIVATE kamailio)
