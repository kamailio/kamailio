cmake_minimum_required(VERSION 3.10)

project(xhttp_pi)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xhttp_pi SHARED ${SRC_FILES})

target_link_libraries(xhttp_pi PRIVATE kamailio LibXml2::LibXml2)
target_link_libraries(xhttp_pi PRIVATE srdb1)
