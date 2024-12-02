cmake_minimum_required(VERSION 3.10)

project(ims_isc)

file(GLOB SRC_FILES "*.c")

add_library(ims_isc SHARED ${SRC_FILES})
target_link_libraries(ims_isc PRIVATE common_modules kamailio)
