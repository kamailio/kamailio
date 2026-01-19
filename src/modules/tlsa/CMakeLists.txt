file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# Find packages
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

target_compile_definitions(${module_name} PRIVATE KSR_LIBSSL_STATIC)
if(LIBSSL_STATIC_SRCLIB)
  target_include_directories(${module_name} PRIVATE ${LIBSSL_STATIC_SRCPATH}/include)
  target_link_directories(${module_name} PRIVATE ${LIBSSL_STATIC_SRCPATH})
else()
  # Static linking with system libraries Note: This assumes the static
  # libraries are installed in a standard location
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})

  find_library(SSL_ALIB libssl.a REQUIRED)
  find_library(CRYPTO_ALIB libcrypto.a REQUIRED)
  find_library(Z_ALIB libz.a REQUIRED)
  #set(OPENSSL_USE_STATIC_LIBS TRUE)
  #find_package(OpenSSL REQUIRED)
  # TODO: Check if this is needed: -Wl,-Bstatic
  #target_link_libraries(${module_name} PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
  target_link_libraries(${module_name} PRIVATE ${SSL_ALIB} ${CRYPTO_ALIB} ${Z_ALIB})
endif()
