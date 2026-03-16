cmake_minimum_required(VERSION 3.13)

# 1. Option Toggle
option(WOLFSSL_INTERNAL "Build using vendored wolfSSL source" OFF)

file(GLOB MODULE_SOURCES "*.c")
add_library(${module_name} SHARED ${MODULE_SOURCES})

if(WOLFSSL_INTERNAL)
    include(ExternalProject)
    
    set(WOLFSSL_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../misc/external/wolfssl/wolfssl")
    set(WOLFSSL_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/wolfssl_vendor")
    set(WOLFSSL_LIB "${WOLFSSL_INSTALL_DIR}/lib/libwolfssl.a")
    set(WOLFSSL_INCLUDE_DIR "${WOLFSSL_INSTALL_DIR}/include")

    # 2. External Build
    ExternalProject_Add(wolfssl_external
        SOURCE_DIR "${WOLFSSL_SRC_DIR}"
        PREFIX "${WOLFSSL_INSTALL_DIR}"
        # Build in source for Autotools consistency
        BINARY_DIR "${WOLFSSL_SRC_DIR}"
        CONFIGURE_COMMAND 
            env -u DEFS -u CFLAGS -u LDFLAGS -u LIBS 
            <SOURCE_DIR>/autogen.sh
            COMMAND
            env -u DEFS -u CFLAGS -u LDFLAGS -u LIBS 
            <SOURCE_DIR>/configure 
            --prefix=${WOLFSSL_INSTALL_DIR}
            --exec-prefix=${WOLFSSL_INSTALL_DIR}
            --enable-all --enable-pkcs11 --enable-static --enable-writedup
            --disable-shared --disable-examples --disable-jni --disable-crl-monitor
            "EXTRA_CFLAGS=-g -O2 -fPIC"
        BUILD_COMMAND $(MAKE)
        INSTALL_COMMAND $(MAKE) install
        BUILD_BYPRODUCTS "${WOLFSSL_LIB}"
    )

    add_library(wolfssl::wolfssl STATIC IMPORTED GLOBAL)
    file(MAKE_DIRECTORY "${WOLFSSL_INCLUDE_DIR}")
    
    set_target_properties(wolfssl::wolfssl PROPERTIES
        IMPORTED_LOCATION "${WOLFSSL_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${WOLFSSL_INCLUDE_DIR}"
    )
    
    add_dependencies(${module_name} wolfssl_external)

else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WOLFSSL REQUIRED IMPORTED_TARGET wolfssl)
    if(NOT TARGET wolfssl::wolfssl)
        add_library(wolfssl::wolfssl ALIAS PkgConfig::WOLFSSL)
    endif()
endif()

# 3. Target Configuration
target_link_libraries(${module_name} PRIVATE wolfssl::wolfssl)

# IMPORTANT: Remove WOLFSSL_USER_SETTINGS if you don't have a local header.
# We add OPENSSL_EXTRA because Kamailio's code expects the OpenSSL-compatible API.
target_compile_definitions(${module_name} PRIVATE 
    OPENSSL_EXTRA
    WOLFSSL_DES_ECB # Example of specific fix for DH/DES errors
  )

