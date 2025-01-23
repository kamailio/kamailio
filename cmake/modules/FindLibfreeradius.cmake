# - Find freeradius-client
# Find the native freeradius-client includes and library.
# Once done this will define
#
#  LIBFREERADIUS_CLIENT_INCLUDE_DIRS - where to find freeradius-client.h, etc.
#  LIBFREERADIUS_CLIENT_LIBRARIES    - List of libraries when using libfreeradius.
#  LIBFREERADIUS_CLIENT_FOUND        - True if libfreeradius found.

find_path(Libfreeradius_INCLUDE_DIR NAMES freeradius-client.h)

find_library(
  Libfreeradius_LIBRARY
  NAMES freeradius-client freeradius-eap
  PATH_SUFFIXES freeradius)

mark_as_advanced(Libfreeradius_LIBRARY Libfreeradius_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set LIBFREERADIUS_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Libfreeradius REQUIRED_VARS LIBFREERADIUS_LIBRARY LIBFREERADIUS_INCLUDE_DIR)

if(Libfreeradius_FOUND)
  find_package_message(
    Libfreeradius "Found Libfreeradius: ${LIBFREERADIUS_LIBRARY}"
    "[${LIBFREERADIUS_LIBRARY}][${LIBFREERADIUS_INCLUDE_DIR}]")
  set(LIBFREERADIUS_INCLUDE_DIRS ${LIBFREERADIUS_INCLUDE_DIR})
  set(LIBFREERADIUS_LIBRARIES ${LIBFREERADIUS_LIBRARY})
  add_library(Libfreeradius::LIBFREERADIUS UNKNOWN IMPORTED)
  set_target_properties(
    Libfreeradius::LIBFREERADIUS PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                            "${LIBFREERADIUS_INCLUDE_DIR}")
  set_target_properties(Libfreeradius::LIBFREERADIUS
                        PROPERTIES IMPORTED_LOCATION "${LIBFREERADIUS_LIBRARY}")
endif()
