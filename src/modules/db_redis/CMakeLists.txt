file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(hiredis REQUIRED IMPORTED_TARGET hiredis)
add_library(hiredis::hiredis ALIAS PkgConfig::hiredis)

target_link_libraries(${module_name} PRIVATE hiredis::hiredis)

# Check for hiredis SSL support
pkg_check_modules(hiredis_ssl IMPORTED_TARGET hiredis_ssl)

if(hiredis_ssl_FOUND)
  # https://github.com/redis/hiredis?tab=readme-ov-file#using-it
  find_package(OpenSSL REQUIRED)
  target_link_libraries(PkgConfig::hiredis_ssl INTERFACE OpenSSL::SSL OpenSSL::Crypto)
  # hiredis_ssl found via pkg-config
  add_library(hiredis::hiredis_ssl ALIAS PkgConfig::hiredis_ssl)
  target_compile_definitions(${module_name} PRIVATE WITH_SSL)
  target_link_libraries(${module_name} PRIVATE hiredis::hiredis_ssl)
endif()

# TODO: Revise when this definition is needed
# ifeq (,$(findstring hiredis,$(HIREDISDEFS)))
# 	DEFS+=-DWITH_HIREDIS_PATH
# endif

# TODO: Check with Makefile for cluster stuff
