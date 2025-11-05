file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(gcrypt IMPORTED_TARGET libgcrypt)

# Fallback to libgcrypt-config if pkg-config fails (RHEL8-based systems)
if(NOT gcrypt_FOUND)
  message(STATUS "Trying to find libgcrypt-config instead...")
  find_program(LIBGCRYPT_CONFIG_EXECUTABLE NAMES libgcrypt-config)
  if(NOT LIBGCRYPT_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "libgcrypt-config not found. Please install libgcrypt development package.")
  endif()
  message(STATUS "Trying to find libgcrypt-config... - Found: ${LIBGCRYPT_CONFIG_EXECUTABLE}")
  execute_process(
    COMMAND ${LIBGCRYPT_CONFIG_EXECUTABLE} --cflags
    OUTPUT_VARIABLE LIBGCRYPT_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND ${LIBGCRYPT_CONFIG_EXECUTABLE} --libs
    OUTPUT_VARIABLE LIBGCRYPT_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  # Create an interface library to hold the compile options and link libraries
  # These will contain already the necessary -I and -L flags.
  # Might need to remove for non-gcc compatible compilers
  add_library(gcrypt_libs INTERFACE)
  set_target_properties(
    gcrypt_libs PROPERTIES INTERFACE_COMPILE_OPTIONS "${LIBGCRYPT_CFLAGS}" INTERFACE_LINK_LIBRARIES
                                                                           "${LIBGCRYPT_LIBS}"
  )
  add_library(gcrypt::gcrypt ALIAS gcrypt_libs)
else()
  add_library(gcrypt::gcrypt ALIAS PkgConfig::gcrypt)
endif()

target_link_libraries(${module_name} PRIVATE gcrypt::gcrypt)
