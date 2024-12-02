cmake_minimum_required(VERSION 3.10)

project(geoip2)

file(GLOB SRC_FILES "*.c")

find_package(PkgConfig REQUIRED)
pkg_check_modules(geoip2 REQUIRED IMPORTED_TARGET libmaxminddb)

add_library(geoip2 SHARED ${SRC_FILES})

target_include_directories(geoip2 PRIVATE ${geoip2_INCLUDE_DIRS})
target_link_libraries(geoip2 PRIVATE kamailio ${geip2_LIBRARIES})
