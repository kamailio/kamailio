find_package(pcre2 QUIET)
if(NOT pcre2_FOUND)
  message(STATUS "PCRE2 library not found... looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  #   TODO: verify we want 8-bit libpcre2
  pkg_check_modules(pcre2 REQUIRED IMPORTED_TARGET libpcre2-8)
  add_library(PCRE2::8BIT ALIAS PkgConfig::pcre2)
endif()

file(GLOB SRC_FILES "*.c")

add_library(lcr SHARED ${SRC_FILES})

target_link_libraries(lcr PRIVATE PCRE2::8BIT)

target_compile_options(lcr PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/lcr/=")
