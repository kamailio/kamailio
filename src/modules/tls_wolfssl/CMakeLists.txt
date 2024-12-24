file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# TODO: Allow wolfSSL from sources
find_package(PkgConfig)
pkg_check_modules(WOLFSSL REQUIRED IMPORTED_TARGET wolfssl)
add_library(wolfssl::wolfssl ALIAS PkgConfig::WOLFSSL)

# TODO: Compile error about DH
target_link_libraries(${module_name} PRIVATE wolfssl::wolfssl)
