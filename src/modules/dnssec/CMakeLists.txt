cmake_minimum_required(VERSION 3.10)

project(dnssec)

file(GLOB SRC_FILES "*.c")

add_library(dnssec SHARED ${SRC_FILES})
target_link_libraries(dnssec PRIVATE common_modules kamailio)
