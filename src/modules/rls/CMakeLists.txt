cmake_minimum_required(VERSION 3.10)

project(rls)

find_package(LibXml2 REQUIRED)

# TODO: Only for darwin?
find_package(ICU QUIET COMPONENTS uc)

file(GLOB SRC_FILES "*.c")

add_library(rls SHARED ${SRC_FILES})

target_link_libraries(rls PRIVATE kamailio LibXml2::LibXml2 ICU::uc)
