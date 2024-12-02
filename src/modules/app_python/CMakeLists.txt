cmake_minimum_required(VERSION 3.10)

project(app_python)

file(GLOB SRC_FILES "*.c")

# python2-dev package is required for Python2::Module target
find_package(Python2 REQUIRED COMPONENTS Development)

add_library(app_python SHARED ${SRC_FILES})
target_link_libraries(app_python PRIVATE kamailio Python2::Module)
