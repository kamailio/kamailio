file(GLOB MODULE_SOURCES "*.c")

find_package(CURL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libsecp256k1 REQUIRED IMPORTED_TARGET libsecp256k1>=0.5)
add_library(libsecp256k1::secp256k1 ALIAS PkgConfig::libsecp256k1)

add_library(${module_name} SHARED ${MODULE_SOURCES})

target_link_libraries(${module_name} PRIVATE CURL::libcurl libsecp256k1::secp256k1)
