file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# Options
# TLS library: OPENSSL (default) or AWSLC (AWS-LC; tcp_main_threads 1 or 2)
set(TLS_BUILD_WITH
    "OPENSSL"
    CACHE STRING "TLS library for the tls module: OPENSSL or AWSLC"
)
set_property(CACHE TLS_BUILD_WITH PROPERTY STRINGS OPENSSL AWSLC)
# AWS-LC outside the system paths (e.g. /opt/aws-lc): searched for tls only;
# CMAKE_PREFIX_PATH would also move the other OpenSSL users onto it
set(TLS_AWSLC_ROOT
    ""
    CACHE PATH "AWS-LC install prefix for TLS_BUILD_WITH=AWSLC (default: system packages)"
)
if(NOT TLS_BUILD_WITH MATCHES "^(OPENSSL|AWSLC)$")
  message(FATAL_ERROR "tls: TLS_BUILD_WITH must be OPENSSL or AWSLC, not '${TLS_BUILD_WITH}'")
endif()
option(LIBSSL_STATIC "Link with static libraries" OFF)
option(LIBSSL_STATIC_SRCLIB "Link with static libraries compiled from source" OFF)
set(LIBSSL_STATIC_SRCPATH
    "/usr/local/src/openssl"
    CACHE PATH "Path to the folder with static libraries compiled from source"
)

# Find packages
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# TODO: Probably all thesse are not needed because if OpenSSL can't be found it
# will fail and ask to set variables like OPENSSL_ROOT_DIR, OPENSSL_INCLUDE_DIR
# and LIBRARIES
if(TLS_BUILD_WITH STREQUAL "AWSLC")
  if(LIBSSL_STATIC)
    message(FATAL_ERROR "tls: LIBSSL_STATIC applies to OpenSSL; unset it for TLS_BUILD_WITH=AWSLC")
  endif()
  # TLS_AWSLC_ROOT: the upstream CMake packages ssl/crypto under it; else
  # distribution packages (upstream ENABLE_DIST_PKG) by pkg-config aws-lc
  if(NOT TLS_AWSLC_ROOT)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(AWSLC IMPORTED_TARGET aws-lc)
    endif()
  endif()
  if(AWSLC_FOUND)
    set(TLS_AWSLC_LIBS PkgConfig::AWSLC)
    set(TLS_AWSLC_INCLUDES ${AWSLC_INCLUDE_DIRS})
    message(STATUS "tls: AWS-LC via pkg-config aws-lc (${AWSLC_LINK_LIBRARIES})")
  else()
    if(TLS_AWSLC_ROOT)
      # crypto first: ssl-config.cmake's find_dependency(crypto) reuses crypto_DIR
      find_package(crypto CONFIG PATHS ${TLS_AWSLC_ROOT} NO_DEFAULT_PATH)
      find_package(ssl CONFIG PATHS ${TLS_AWSLC_ROOT} NO_DEFAULT_PATH)
    else()
      find_package(ssl CONFIG)
    endif()
    if(NOT ssl_FOUND)
      message(
        FATAL_ERROR "tls: AWS-LC not found: install its development package (pkg-config aws-lc)"
                    " or set TLS_AWSLC_ROOT to its prefix (CMake package ssl)"
      )
    endif()
    set(TLS_AWSLC_LIBS AWS::ssl AWS::crypto)
    get_target_property(TLS_AWSLC_INCLUDES AWS::crypto INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "tls: AWS-LC via CMake package ssl (${ssl_DIR})")
  endif()
  # the headers must be AWS-LC: a plain OpenSSL under the same names would link
  include(CheckSymbolExists)
  set(CMAKE_REQUIRED_INCLUDES ${TLS_AWSLC_INCLUDES})
  check_symbol_exists(OPENSSL_IS_AWSLC "openssl/base.h" TLS_HAVE_AWSLC)
  unset(CMAKE_REQUIRED_INCLUDES)
  if(NOT TLS_HAVE_AWSLC)
    message(
      FATAL_ERROR "tls: OPENSSL_IS_AWSLC missing in ${TLS_AWSLC_INCLUDES}: not AWS-LC headers"
    )
  endif()
  target_link_libraries(${module_name} PRIVATE ${TLS_AWSLC_LIBS} ZLIB::ZLIB Threads::Threads)
  # libraries outside the system paths (e.g. /opt/aws-lc): keep them in the
  # RPATH, else libssl.so may resolve to the system OpenSSL
  set_target_properties(${module_name} PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)
elseif(LIBSSL_STATIC)
  target_compile_definitions(${module_name} PRIVATE KSR_LIBSSL_STATIC)
  if(LIBSSL_STATIC_SRCLIB)
    target_include_directories(${module_name} PRIVATE ${LIBSSL_STATIC_SRCPATH}/include)
    target_link_directories(${module_name} PRIVATE ${LIBSSL_STATIC_SRCPATH})
  else()
    # Static linking with system libraries Note: This assumes the static
    # libraries are installed in a standard location
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    find_package(OpenSSL REQUIRED)
    # TODO: Check if this is needed: -Wl,-Bstatic
    target_link_libraries(${module_name} PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
  endif()
else()
  find_package(OpenSSL REQUIRED)
  target_link_libraries(
    ${module_name} PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB Threads::Threads
  )
endif()

# Install the configuration file (tls.cfg) using a CODE block to check
# existence at install time instead of configure time
install(
  CODE "
    set(dir \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\")
    set(sample_file \"tls.cfg.sample\")
    set(file \"tls.cfg\")

    if(EXISTS \"\${dir}/\${file}\")
        message(STATUS \"\${file} already exists in \${dir}/\${file}.
          Installing as \${file}.sample\")
        file(INSTALL \"${CMAKE_CURRENT_BINARY_DIR}/\${sample_file}\"
          DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    else()
        file(INSTALL \"${CMAKE_CURRENT_BINARY_DIR}/\${sample_file}\"
            DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
            RENAME \"\${file}\"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
"
  COMPONENT ${group_name}
)

add_custom_command(
  OUTPUT tls.cfg.sample
  COMMAND
    sed -e "s#\/usr/local/etc/kamailio/#${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/#g" -e
    "s#kamailio-selfsigned#${MAIN_NAME}-selfsigned#g" < tls.cfg >
    ${CMAKE_CURRENT_BINARY_DIR}/tls.cfg.sample
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Always build/generate tls.cfg.sample in the build directory
# This is needed for the default install target to pick it up
add_custom_target(tls_cfg_sample ALL DEPENDS tls.cfg.sample)

add_custom_target(
  tls-install-cfg
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/tls.cfg.sample
          $ENV{DESTDIR}${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Installing tls.cfg.sample to ${MAIN_NAME} config directory"
)
add_dependencies(tls-install-cfg tls_cfg_sample)

# Add tls-cert target
add_custom_target(
  tls-install-cert
  COMMAND ${CMAKE_COMMAND} -E env MAIN_NAME=${MAIN_NAME} ./tls_cert.sh -d
          ${CMAKE_INSTALL_FULL_SYSCONFDIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating self-signed certificates for ${MAIN_NAME}"
)
