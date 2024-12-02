cmake_minimum_required(VERSION 3.10)

project(presence_dialoginfo)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(presence_dialoginfo SHARED ${SRC_FILES})
target_link_libraries(presence_dialoginfo PRIVATE kamailio LibXml2::LibXml2)
