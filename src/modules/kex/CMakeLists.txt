cmake_minimum_required(VERSION 3.10)
project(kex)

# Set the source files
file(GLOB SRC_FILES *.c)
# message(STATUS "Source files: ${SRC_FILES}")
add_library(kex SHARED ${SRC_FILES})
target_link_libraries(kex common common_modules kamailio)
target_include_directories(kex PUBLIC ${CMAKE_SOURCE_DIR}/src/core)
