cmake_minimum_required(VERSION 3.10)

project(ims_auth)

file(GLOB SRC_FILES "*.c")

add_library(ims_auth SHARED ${SRC_FILES})
target_link_libraries(ims_auth PRIVATE common_modules kamailio)
