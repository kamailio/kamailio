cmake_minimum_required(VERSION 3.10)

project(xmpp)

file(GLOB SRC_FILES "*.c")

add_library(xmpp SHARED ${SRC_FILES})
target_link_libraries(xmpp PRIVATE common_modules kamailio)
