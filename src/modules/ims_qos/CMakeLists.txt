cmake_minimum_required(VERSION 3.10)

project(ims_qos)

file(GLOB SRC_FILES "*.c")

add_library(ims_qos SHARED ${SRC_FILES})
target_link_libraries(ims_qos PRIVATE common_modules kamailio)
