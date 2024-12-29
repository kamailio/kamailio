file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

target_link_libraries(${module_name} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
