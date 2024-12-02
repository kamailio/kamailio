cmake_minimum_required(VERSION 3.10)

project(xmlrpc)

file(GLOB SRC_FILES "*.c")

add_library(xmlrpc SHARED ${SRC_FILES})
target_link_libraries(xmlrpc PRIVATE common_modules kamailio)
