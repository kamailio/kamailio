file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(hiredis REQUIRED IMPORTED_TARGET hiredis)
add_library(hiredis::hiredis ALIAS PkgConfig::hiredis)

target_link_libraries(${module_name} PRIVATE hiredis::hiredis)

# Check for hiredis SSL support
pkg_check_modules(hiredis_ssl IMPORTED_TARGET hiredis_ssl)

if(hiredis_ssl_FOUND)
  find_package(OpenSSL REQUIRED)
  # hiredis_ssl found via pkg-config
  add_library(hiredis::hiredis_ssl ALIAS PkgConfig::hiredis_ssl)
  target_compile_definitions(${module_name} PRIVATE WITH_SSL)
  target_link_libraries(${module_name} PRIVATE hiredis::hiredis_ssl)
  # https://github.com/redis/hiredis?tab=readme-ov-file#using-it
  target_link_libraries(${module_name} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# TODO: Revise when this definition is needed
# ifeq (,$(findstring hiredis,$(HIREDISDEFS)))
# 	DEFS+=-DWITH_HIREDIS_PATH
# endif

# hiredis_cluster checks
pkg_check_modules(hiredis_cluster IMPORTED_TARGET hiredis_cluster)
if(hiredis_cluster_FOUND)
  # Check for hiredis_cluster SSL support
  pkg_check_modules(hiredis_cluster_ssl IMPORTED_TARGET hiredis_cluster_ssl)
  if(hiredis_cluster_ssl_FOUND)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(PkgConfig::hiredis_cluster_ssl INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    # hiredis_cluster_ssl found via pkg-config
    add_library(hiredis::hiredis_cluster_ssl ALIAS PkgConfig::hiredis_cluster_ssl)
    target_compile_definitions(${module_name} PRIVATE WITH_SSL)
    target_link_libraries(${module_name} PRIVATE hiredis::hiredis_cluster_ssl)
  endif()
  add_library(hiredis::hiredis_cluster ALIAS PkgConfig::hiredis_cluster)
  target_compile_definitions(${module_name} PRIVATE WITH_HIREDIS_CLUSTER)
  target_link_libraries(${module_name} PRIVATE hiredis::hiredis_cluster)
endif()
