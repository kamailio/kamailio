cmake_minimum_required(VERSION 3.10)
project(kex)

file(GLOB SRC_FILES *.c)

add_library(kex SHARED ${SRC_FILES})
target_link_libraries(kex PRIVATE kamailio)
# target_include_directories(kex PUBLIC ${CMAKE_SOURCE_DIR}/src/core)
