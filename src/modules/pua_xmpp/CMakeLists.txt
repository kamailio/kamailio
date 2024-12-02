cmake_minimum_required(VERSION 3.10)

project(pua_xmpp)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_xmpp SHARED ${SRC_FILES})
target_link_libraries(pua_xmpp PRIVATE kamailio LibXml2::LibXml2)
