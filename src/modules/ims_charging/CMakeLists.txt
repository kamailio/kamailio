cmake_minimum_required(VERSION 3.10)

project(ims_charging)

file(GLOB SRC_FILES "*.c")

add_library(ims_charging SHARED ${SRC_FILES})

target_link_libraries(ims_charging PRIVATE kamailio)
