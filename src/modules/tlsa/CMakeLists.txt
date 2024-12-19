# Find packages
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(tlsa STATIC ${SRC_FILES})

# TODO: Probably all thesse are not need because if OpenSSL can't be found
# it will fail and ask to set variables like OPENSSL_ROOT_DIR, OPENSSL_INCLUDE_DIR and LIBRARIES
if(LIBSSL_STATIC)
  target_compile_definitions(tlsa KSR_LIBSSL_STATIC)
  if(LIBSSL_STATIC_SRCLIB)
    target_include_directories(tlsa PRIVATE ${LIBSSL_STATIC_SRCPATH}/include)
    target_link_directories(tlsa PRIVATE ${LIBSSL_STATIC_SRCPATH})
  else()
    # Static linking with system libraries
    # Note: This assumes the static libraries are installed in a standard location
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    find_package(OpenSSL REQUIRED)
    # TODO: Check if this is needed: -Wl,-Bstatic
    target_link_libraries(tlsa PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
  endif()
else()
  find_package(OpenSSL REQUIRED)
  target_link_libraries(
    tlsa PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB Threads::Threads
  )
endif()

# target_link_libraries(.*PRIVATE)

target_compile_options(tlsa PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/tlsa/=")
