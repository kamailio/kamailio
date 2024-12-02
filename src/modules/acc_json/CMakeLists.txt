cmake_minimum_required(VERSION 3.10)

project(acc_json)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED jansson)

file(GLOB SRC_FILES "*.c")

add_library(acc_json SHARED ${SRC_FILES})
target_link_libraries(acc_json PRIVATE kamailio ${JANSON_LIBRARIES})
