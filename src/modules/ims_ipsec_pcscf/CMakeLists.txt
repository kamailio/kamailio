cmake_minimum_required(VERSION 3.10)

project(ims_ipsec_pcscf)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libmnl REQUIRED IMPORTED_TARGET libmnl)
add_library(libmnl::libmnl ALIAS PkgConfig::libmnl)

file(GLOB SRC_FILES "*.c")

add_library(ims_ipsec_pcscf SHARED ${SRC_FILES})

target_link_libraries(ims_ipsec_pcscf PRIVATE kamailio libmnl::libmnl)
