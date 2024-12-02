# 3.12 For libxml2 target
cmake_minimum_required(VERSION 3.12)

project(presence)

find_package(LibXml2 REQUIRED)
find_package(ICU QUIET COMPONENTS uc)

file(GLOB SRC_FILES "*.c")

add_library(presence SHARED ${SRC_FILES})

target_link_libraries(presence PRIVATE kamailio LibXml2::LibXml2 ICU::uc)
target_link_libraries(presence PRIVATE srdb1)
