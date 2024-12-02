cmake_minimum_required(VERSION 3.10)

project(cplc)

file(GLOB SRC_FILES "*.c")

find_package(LibXml2 REQUIRED)

add_library(cplc SHARED ${SRC_FILES})
target_link_libraries(cplc PRIVATE kamailio LibXml2::LibXml2)

target_link_libraries(cplc PRIVATE srdb1)
