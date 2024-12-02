cmake_minimum_required(VERSION 3.10)

project(xmlrpc)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xmlrpc SHARED ${SRC_FILES})

target_compile_definitions(xmlrpc PRIVATE XMLRPC_SYSTEM_MALLOC)
target_link_libraries(xmlrpc PRIVATE kamailio LibXml2::LibXml2)
