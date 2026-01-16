file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# unset to avoid finding shared library from previous find_package calls
# that are cached
unset(OPENSSL_CRYPTO_LIBRARY CACHE)
unset(OPENSSL_INCLUDE_DIR CACHE)
unset(OPENSSL_SSL_LIBRARY CACHE)
set(OPENSSL_USE_STATIC_LIBS TRUE)

# Find packages
find_package(Threads REQUIRED)

target_compile_definitions(${module_name} PRIVATE KSR_LIBSSL_STATIC)
if(LIBSSL_STATIC_SRCLIB)
  target_include_directories(${module_name} PRIVATE ${LIBSSL_STATIC_SRCPATH}/include)
  target_link_directories(${module_name} PRIVATE ${LIBSSL_STATIC_SRCPATH})
else()
  # Static linking with system libraries Note: This assumes the static
  # libraries are installed in a standard location
  find_package(OpenSSL REQUIRED)

  # TODO: Check if this is needed: -Wl,-Bstatic
  target_link_libraries(${module_name} PRIVATE OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
endif()

# Reset variables to avoid interference with other modules
unset(OPENSSL_CRYPTO_LIBRARY CACHE)
unset(OPENSSL_INCLUDE_DIR CACHE)
unset(OPENSSL_SSL_LIBRARY CACHE)
set(OPENSSL_USE_STATIC_LIBS FALSE)
