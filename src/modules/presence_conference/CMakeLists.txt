cmake_minimum_required(VERSION 3.10)

project(presence_conference)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(presence_conference SHARED ${SRC_FILES})
target_link_libraries(presence_conference PRIVATE kamailio LibXml2::LibXml2)
