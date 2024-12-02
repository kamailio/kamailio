cmake_minimum_required(VERSION 3.10)

project(presence_reginfo)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(presence_reginfo SHARED ${SRC_FILES})
target_link_libraries(presence_reginfo PRIVATE kamailio LibXml2::LibXml2)
