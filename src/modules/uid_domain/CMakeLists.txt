cmake_minimum_required(VERSION 3.10)

project(uid_domain)

file(GLOB SRC_FILES "*.c")

add_library(uid_domain SHARED ${SRC_FILES})
target_link_libraries(uid_domain PRIVATE common_modules kamailio)
