cmake_minimum_required(VERSION 3.10)

project(sipcapture)

file(GLOB SRC_FILES "*.c")

add_library(sipcapture SHARED ${SRC_FILES})
target_link_libraries(sipcapture PRIVATE common_modules kamailio)

target_link_libraries(sipcapture PRIVATE srdb1)
