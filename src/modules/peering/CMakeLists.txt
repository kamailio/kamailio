cmake_minimum_required(VERSION 3.10)

project(peering)

find_package(Radius REQUIRED)
file(GLOB SRC_FILES "*.c")

add_library(peering SHARED ${SRC_FILES})
target_compile_definitions(peering PRIVATE ${RADIUSCLIENT_LIB})

target_link_libraries(peering PRIVATE kamailio)
