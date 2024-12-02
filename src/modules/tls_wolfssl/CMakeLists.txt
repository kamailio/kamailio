cmake_minimum_required(VERSION 3.10)

project(tls_wolfssl)

# TODO: Allow wolfSSL from sources
find_package(PkgConfig)
pkg_check_modules(WOLFSSL REQUIRED IMPORTED_TARGET wolfssl)
add_library(wolfssl::wolfssl ALIAS PkgConfig::WOLFSSL)

file(GLOB SRC_FILES "*.c")

add_library(tls_wolfssl SHARED ${SRC_FILES})

# TODO: Compile error about DH
target_link_libraries(tls_wolfssl PRIVATE kamailio wolfssl::wolfssl)
