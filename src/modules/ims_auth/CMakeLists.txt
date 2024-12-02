cmake_minimum_required(VERSION 3.10)

project(ims_auth)

find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(ims_auth SHARED ${SRC_FILES})
target_link_libraries(ims_auth PRIVATE kamailio LibXml2::LibXml2 kamailio_ims)
