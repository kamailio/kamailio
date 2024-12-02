cmake_minimum_required(VERSION 3.10)

project(xmpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(expat REQUIRED IMPORTED_TARGET expat)
add_library(expat::expat ALIAS PkgConfig::expat)

file(GLOB SRC_FILES "*.c")

add_library(xmpp SHARED ${SRC_FILES})

target_link_libraries(xmpp PRIVATE kamailio expat::expat)
