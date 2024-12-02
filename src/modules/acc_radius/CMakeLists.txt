cmake_minimum_required(VERSION 3.10)

project(acc_radius)

file(GLOB SRC_FILES "*.c")

find_package(Radius REQUIRED)

add_library(acc_radius SHARED ${SRC_FILES})
target_compile_definitions(acc_radius PRIVATE ${RADIUSCLIENT_LIB})
target_link_libraries(acc_radius PRIVATE kamailio)
