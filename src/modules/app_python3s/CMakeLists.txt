cmake_minimum_required(VERSION 3.10)

project(app_python3s)

file(GLOB SRC_FILES "*.c")

find_package(Python3 REQUIRED COMPONENTS Development)

add_library(app_python3s STATIC ${SRC_FILES})
target_link_libraries(app_python3s PRIVATE kamailio Python3::Module)
