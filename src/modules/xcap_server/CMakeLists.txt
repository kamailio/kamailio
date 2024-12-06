cmake_minimum_required(VERSION 3.10)

project(xcap_server)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xcap_server SHARED ${SRC_FILES})

target_link_libraries(xcap_server PRIVATE kamailio LibXml2::LibXml2)
