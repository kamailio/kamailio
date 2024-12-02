cmake_minimum_required(VERSION 3.10)

project(ims_icscf)

file(GLOB SRC_FILES "*.c")

add_library(ims_icscf SHARED ${SRC_FILES})
target_link_libraries(ims_icscf PRIVATE common_modules kamailio)

target_link_libraries(ims_icscf PRIVATE srdb1)
