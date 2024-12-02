cmake_minimum_required(VERSION 3.10)

project(pua_bla)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_bla SHARED ${SRC_FILES})
target_link_libraries(pua_bla PRIVATE kamailio LibXml2::LibXml2)
