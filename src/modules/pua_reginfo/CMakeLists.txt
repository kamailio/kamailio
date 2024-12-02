cmake_minimum_required(VERSION 3.10)

project(pua_reginfo)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_reginfo SHARED ${SRC_FILES})
target_link_libraries(pua_reginfo PRIVATE kamailio LibXml2::LibXml2)
