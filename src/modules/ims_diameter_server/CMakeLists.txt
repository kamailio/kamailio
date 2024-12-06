cmake_minimum_required(VERSION 3.10)

project(ims_diameter_server)

file(GLOB SRC_FILES "*.c")

add_library(ims_diameter_server SHARED ${SRC_FILES})

target_link_libraries(ims_diameter_server PRIVATE kamailio)
