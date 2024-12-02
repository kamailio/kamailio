cmake_minimum_required(VERSION 3.10)

project(misc_radius)

find_package(Radius REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(misc_radius SHARED ${SRC_FILES})

target_compile_definitions(misc_radius PRIVATE ${RADIUSCLIENT_LIB})
target_link_libraries(misc_radius PRIVATE kamailio)
