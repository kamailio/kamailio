# 3.12 For libxml2 target
cmake_minimum_required(VERSION 3.12)

file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(LibXml2 REQUIRED)
find_package(ICU QUIET COMPONENTS uc)

target_link_libraries(${module_name} PRIVATE LibXml2::LibXml2 ICU::uc)
