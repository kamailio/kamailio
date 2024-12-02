cmake_minimum_required(VERSION 3.10)

project(registrar)

file(GLOB SRC_FILES "*.c")

add_library(registrar SHARED ${SRC_FILES})
target_link_libraries(registrar PRIVATE kamailio)
# target_compile_definitions(registrar PRIVATE STATISTICS)
