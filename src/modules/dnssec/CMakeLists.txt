file(GLOB SRC_FILES "*.c")

find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
find_package(Threads REQUIRED)

add_library(dnssec SHARED ${SRC_FILES})

# TODO: library libval not foud on recent distributions:
# whichi library is sres?
# Consider depreciating this module?
target_link_libraries(
  dnssec PRIVATE val-threads sres Threads::Threads OpenSSL::SSL OpenSSL::Crypto
)

# target_link_libraries(.*PRIVATE)

target_compile_options(dnssec PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/dnssec/=")
