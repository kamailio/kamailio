file(GLOB MODULE_SOURCES "*.c")

find_package(CURL REQUIRED)
find_package(libsecp256k1 0.5 REQUIRED CONFIG)

add_library(${module_name} SHARED ${MODULE_SOURCES})

target_link_libraries(${module_name} PRIVATE CURL::libcurl libsecp256k1::secp256k1)
