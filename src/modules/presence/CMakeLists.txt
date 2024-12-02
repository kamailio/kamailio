# 3.12 For libxml2 target
cmake_minimum_required(VERSION 3.12)

project(presence)

find_package(LibXml2 REQUIRED)
find_package(ICU QUIET COMPONENTS uc)

file(GLOB SRC_FILES "*.c")

add_library(presence SHARED ${SRC_FILES})
target_link_libraries(presence PRIVATE common common_modules kamailio)

if(LibXml2_FOUND)
    message(STATUS "Using libxml2")
    target_link_libraries(presence PRIVATE LibXml2::LibXml2)
endif()

if(ICU_FOUND)
    message(STATUS "Using ICU")
    target_link_libraries(presence PRIVATE ICU::uc)
endif()