file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(pcre2 QUIET)
if(NOT pcre2_FOUND)
  message(STATUS "PCRE2 library not found... looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  # TODO: verify we want 8-bit libpcre2
  pkg_check_modules(pcre2 REQUIRED IMPORTED_TARGET libpcre2-8)
  add_library(PCRE2::8BIT ALIAS PkgConfig::pcre2)
endif()

target_link_libraries(${module_name} PRIVATE PCRE2::8BIT)
