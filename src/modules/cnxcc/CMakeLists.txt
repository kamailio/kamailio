cmake_minimum_required(VERSION 3.10)

project(cnxcc)

file(GLOB SRC_FILES "*.c")

add_library(cnxcc SHARED ${SRC_FILES})
target_link_libraries(cnxcc PRIVATE common_modules kamailio)

target_link_libraries(cnxcc PRIVATE srdb1)

