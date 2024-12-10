find_package(OpenSSL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(outbound SHARED ${SRC_FILES})
target_link_libraries(outbound PRIVATE OpenSSL::SSL OpenSSL::Crypto)
