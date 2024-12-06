cmake_minimum_required(VERSION 3.10)

project(ims_ocs)

file(GLOB SRC_FILES "*.c")

add_library(ims_ocs SHARED ${SRC_FILES})

target_link_libraries(ims_ocs PRIVATE kamailio)
