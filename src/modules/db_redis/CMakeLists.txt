file(GLOB MODULE_SOURCES "*.c")

option(WITH_SENTINELS "Build db_redis with Redis Sentinel support" OFF)
option(WITH_HIREDIS_CLUSTER "Build db_redis with hiredis cluster support" OFF)

if(WITH_SENTINELS AND WITH_HIREDIS_CLUSTER)
  message(FATAL_ERROR "db_redis: WITH_SENTINELS and WITH_HIREDIS_CLUSTER are mutually exclusive")
endif()

if(NOT WITH_SENTINELS)
  list(REMOVE_ITEM MODULE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/redis_sentinels.c")
endif()

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(hiredis REQUIRED IMPORTED_TARGET hiredis)
add_library(hiredis::hiredis ALIAS PkgConfig::hiredis)

target_link_libraries(${module_name} PRIVATE hiredis::hiredis)

if(WITH_SENTINELS)
  target_compile_definitions(${module_name} PRIVATE WITH_SENTINELS)
endif()

if(WITH_HIREDIS_CLUSTER)
  pkg_check_modules(hiredis_cluster REQUIRED IMPORTED_TARGET hiredis_cluster)
  add_library(hiredis::hiredis_cluster ALIAS PkgConfig::hiredis_cluster)
  target_compile_definitions(${module_name} PRIVATE WITH_HIREDIS_CLUSTER)
  target_link_libraries(${module_name} PRIVATE hiredis::hiredis_cluster)
endif()

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
