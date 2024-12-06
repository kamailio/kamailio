# 3.12 For libxml2 target
cmake_minimum_required(VERSION 3.12)

project(presence_xml)

find_package(LibXml2 REQUIRED)
find_package(ICU QUIET COMPONENTS uc)

file(GLOB SRC_FILES "*.c")

add_library(presence_xml SHARED ${SRC_FILES})
target_link_libraries(presence_xml PRIVATE kamailio LibXml2::LibXml2 ICU::uc)
