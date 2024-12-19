# Options
option(LIBSSL_STATIC "Link with static libraries" OFF)
option(LIBSSL_STATIC_SRCLIB "Link with static libraries compiled from source"
       OFF)
set(LIBSSL_STATIC_SRCPATH
    "/usr/local/src/openssl"
    CACHE PATH "Path to the folder with static libraries compiled from source")

# Find packages
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(tls SHARED ${SRC_FILES})

# TODO: Probably all thesse are not need because if OpenSSL can't be found it
# will fail and ask to set variables like OPENSSL_ROOT_DIR, OPENSSL_INCLUDE_DIR
# and LIBRARIES
if(LIBSSL_STATIC)
  target_compile_definitions(tls KSR_LIBSSL_STATIC)
  if(LIBSSL_STATIC_SRCLIB)
    target_include_directories(tls PRIVATE ${LIBSSL_STATIC_SRCPATH}/include)
    target_link_directories(tls PRIVATE ${LIBSSL_STATIC_SRCPATH})
  else()
    # Static linking with system libraries Note: This assumes the static
    # libraries are installed in a standard location
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    find_package(OpenSSL REQUIRED)
    # TODO: Check if this is needed: -Wl,-Bstatic
    target_link_libraries(tls PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
  endif()
else()
  find_package(OpenSSL REQUIRED)
  target_link_libraries(tls PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB
                                    Threads::Threads)
endif()

# target_link_libraries(tls PRIVATE ${CMAKE_THREAD_LIBS_INIT})

# TODO: Install files such as cfg and modify it with sed and certs
