cmake_minimum_required(VERSION 3.10)

project(ims_registrar_pcscf)

file(GLOB SRC_FILES "*.c")

find_package(LibXml2 REQUIRED)

add_library(ims_registrar_pcscf SHARED ${SRC_FILES})
target_link_libraries(ims_registrar_pcscf PRIVATE kamailio LibXml2::LibXml2)
