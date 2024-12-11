# 3.12 For libxml2 target
cmake_minimum_required(VERSION 3.12)

find_package(LibXml2 REQUIRED)
find_package(ICU QUIET COMPONENTS uc)

file(GLOB SRC_FILES "*.c")

add_library(presence SHARED ${SRC_FILES})

target_link_libraries(presence PRIVATE LibXml2::LibXml2 ICU::uc)

target_compile_options(presence PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/presence/=")
