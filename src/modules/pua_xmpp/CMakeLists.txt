cmake_minimum_required(VERSION 3.10)

project(pua_xmpp)

file(GLOB SRC_FILES "*.c")

add_library(pua_xmpp SHARED ${SRC_FILES})
target_link_libraries(pua_xmpp PRIVATE common_modules kamailio)
