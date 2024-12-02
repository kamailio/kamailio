cmake_minimum_required(VERSION 3.10)

project(pua_dialoginfo)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_dialoginfo SHARED ${SRC_FILES})
target_link_libraries(pua_dialoginfo PRIVATE kamailio LibXml2::LibXml2)
