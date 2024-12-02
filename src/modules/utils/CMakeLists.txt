cmake_minimum_required(VERSION 3.10)

project(utils)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(utils SHARED ${SRC_FILES})

target_link_libraries(utils PRIVATE kamailio LibXml2::LibXml2)
target_link_libraries(utils PRIVATE srdb1)
