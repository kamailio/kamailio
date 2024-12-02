cmake_minimum_required(VERSION 3.10)

project(ims_dialog)

file(GLOB SRC_FILES "*.c")

add_library(ims_dialog SHARED ${SRC_FILES})
target_link_libraries(ims_dialog PRIVATE common_modules kamailio)

target_link_libraries(ims_dialog PRIVATE srdb1)
