cmake_minimum_required(VERSION 3.10)

project(mqtt)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libmosquitto REQUIRED IMPORTED_TARGET libmosquitto)
add_library(mosquitto::mosquitto ALIAS PkgConfig::libmosquitto)

find_package(Libev REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(mqtt SHARED ${SRC_FILES})
target_link_libraries(mqtt PRIVATE kamailio mosquitto::mosquitto Libev::Libev)
