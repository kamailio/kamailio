cmake_minimum_required(VERSION 3.10)

project(stirshaken)

find_package(PkgConfig REQUIRED)
pkg_check_modules(stirshaken REQUIRED IMPORTED_TARGET libstireshaken)
add_library(stirshaken::stirshaken ALIAS PkgConfig::stirshaken)

file(GLOB SRC_FILES "*.c")

add_library(stirshaken SHARED ${SRC_FILES})
target_link_libraries(stirshaken PRIVATE kamailio stirshaken::stirshaken)
