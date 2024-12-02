cmake_minimum_required(VERSION 3.10)

project(sms)

file(GLOB SRC_FILES "*.c")

add_library(sms SHARED ${SRC_FILES})
target_link_libraries(sms PRIVATE kamailio)
