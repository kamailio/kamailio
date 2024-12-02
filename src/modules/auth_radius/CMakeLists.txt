cmake_minimum_required(VERSION 3.10)

project(auth_radius)

file(GLOB SRC_FILES "*.c")

find_package(Radius REQUIRED)

add_library(auth_radius SHARED ${SRC_FILES})
target_compile_definitions(auth_radius PRIVATE ${RADIUSCLIENT_LIB})
target_link_libraries(auth_radius PRIVATE kamailio)
