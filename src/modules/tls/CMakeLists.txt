cmake_minimum_required(VERSION 3.10)
project(tls)

# Options
option(LIBSSL_STATIC "Link with static libraries" OFF)
option(LIBSSL_STATIC_SRCLIB "Link with static libraries compiled from source" OFF)
set(LIBSSL_STATIC_SRCPATH "/usr/local/src/openssl" CACHE PATH "Path to the folder with static libraries compiled from source")

# Find packages
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

file(GLOB SRC_FILES "*.c")
# Include directories and link libraries based on options
if(LIBSSL_STATIC)
    if(LIBSSL_STATIC_SRCLIB)
        include_directories(${LIBSSL_STATIC_SRCPATH}/include)
        link_directories(${LIBSSL_STATIC_SRCPATH})
        target_compile_definitions(tls KSR_LIBSSL_STATIC)
    else()
        # Static linking with system libraries
        # Note: This assumes the static libraries are installed in a standard location
        target_link_libraries(tls -Wl,-Bstatic OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB -Wl,-Bdynamic)
    endif()
else()
    include_directories(${OPENSSL_INCLUDE_DIR})
    link_libraries(OpenSSL::SSL OpenSSL::Crypto)
endif()

# Define the target
add_library(tls SHARED ${SRC_FILES}) # Assuming the source file is src/tls_module.c
target_link_libraries(tls common ${CMAKE_THREAD_LIBS_INIT})

#  TODO: Install files such as cfg and modify it with sed and certs