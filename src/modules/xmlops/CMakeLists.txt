cmake_minimum_required(VERSION 3.10)

project(xmlops)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xmlops SHARED ${SRC_FILES})
target_link_libraries(xmlops PRIVATE kamailio LibXml2::LibXml2)
