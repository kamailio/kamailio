cmake_minimum_required(VERSION 3.10)

project(ims_isc)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(ims_isc SHARED ${SRC_FILES})
target_link_libraries(ims_isc PRIVATE kamailio LibXml2::LibXml2)
