cmake_minimum_required(VERSION 3.10)

project(lost)

find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(lost SHARED ${SRC_FILES})
target_link_libraries(lost PRIVATE kamailio CURL::libcurl LibXml2::LibXml2)
