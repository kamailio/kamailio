file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# Options
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
if(LIBSSL_STATIC)
  target_compile_definitions(${module_name} KSR_LIBSSL_STATIC)
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
    set(file \"tls.cfg\")
    if(EXISTS \"\${dir}/\${file}\")
        message(STATUS \"\${file} already exists in \${dir}/\${file}.
          Installing as \${file}.sample\")
        file(INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/\${file}\"
          DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
          RENAME \"\${file}.sample\"
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    else()
        file(INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/\${file}\"
            DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
"
  COMPONENT ${group_name}
)

add_custom_command(
  OUTPUT tls.cfg.sample
  COMMAND
    sed -e "s#\/usr/local/etc/kamailio/#${CMAKE_INSTALL_FULL_SYSCONFDIR}#g" -e
    "s#kamailio-selfsigned#${MAIN_NAME}-selfsigned#g" < tls.cfg >
    ${CMAKE_CURRENT_BINARY_DIR}/tls.cfg.sample
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Always build/generate tls.cfg.sample in the build directory
# This is needed for the default install target to pick it up
add_custom_target(tls_cfg_sample ALL DEPENDS tls.cfg.sample)

add_custom_target(
  tls-install-cfg
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component tls-cfg
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
