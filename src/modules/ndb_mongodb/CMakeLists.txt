file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(mongoc2 IMPORTED_TARGET mongoc2)

if(mongoc2_FOUND)
  add_library(mongoc::mongoc ALIAS PkgConfig::mongoc2)
else()
  pkg_check_modules(mongoc IMPORTED_TARGET libmongoc-1.0>=1.7)
  if(NOT mongoc_FOUND)
    message(FATAL_ERROR "libmongoc-1.0 not found! Please install libmongoc-1.0-dev.")
  endif()
  add_library(mongoc::mongoc ALIAS PkgConfig::mongoc)
  target_compile_definitions(${module_name} PRIVATE HAVE_LIBMONGOC1)
endif()

target_link_libraries(${module_name} PRIVATE mongoc::mongoc)
