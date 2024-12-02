cmake_minimum_required(VERSION 3.10)

project(janssonrpcc)

file(GLOB SRC_FILES "*.c")

find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson)
add_library(jansson::jansson ALIAS PkgConfig::JANSSON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EVENT REQUIRED IMPORTED_TARGET libevent)
add_library(event::event ALIAS PkgConfig::EVENT)

add_library(janssonrpcc SHARED ${SRC_FILES})
target_link_libraries(janssonrpcc PRIVATE jansson::jansson event::event)
