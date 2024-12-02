cmake_minimum_required(VERSION 3.10)

project(outbound)

find_package(OpenSSL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(outbound SHARED ${SRC_FILES})
target_link_libraries(outbound PRIVATE kamailio OpenSSL::SSL OpenSSL::Crypto)
