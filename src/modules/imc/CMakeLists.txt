cmake_minimum_required(VERSION 3.10)

project(imc)

file(GLOB SRC_FILES "*.c")

add_library(imc SHARED ${SRC_FILES})

target_link_libraries(imc PRIVATE kamailio)
target_link_libraries(imc PRIVATE srdb1)
