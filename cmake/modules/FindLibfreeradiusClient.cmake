# - Find freeradius-client
# Find the native freeradius-client includes and library.
# Once done this will define
#
#  Target:
#  LibfreeradiusClient::LIBFREERADIUS

#  LibfreeradiusClient_INCLUDE_DIR(S) - where to find freeradius-client.h, etc.
#  LibfreeradiusClient_LIBRARY(IES)    - List of libraries when using libfreeradius.
#  LibfreeradiusClient_FOUND        - True if libfreeradius found.

find_path(LibfreeradiusClient_INCLUDE_DIR NAMES freeradius-client.h)

find_library(
  LibfreeradiusClient_LIBRARY
  NAMES freeradius-client freeradius-eap
  PATH_SUFFIXES freeradius)

mark_as_advanced(LibfreeradiusClient_LIBRARY LibfreeradiusClient_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set LibfreeradiusClient_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LibfreeradiusClient REQUIRED_VARS LibfreeradiusClient_LIBRARY
                                    LibfreeradiusClient_INCLUDE_DIR)

if(LibfreeradiusClient_FOUND)
  find_package_message(
    LibfreeradiusClient "Found Libfreeradius: ${LibfreeradiusClient_LIBRARY}"
    "[${LibfreeradiusClient_LIBRARY}][${LibfreeradiusClient_INCLUDE_DIR}]")
  set(LibfreeradiusClient_INCLUDE_DIRS ${LibfreeradiusClient_INCLUDE_DIR})
  set(LibfreeradiusClient_LIBRARIES ${LibfreeradiusClient_LIBRARY})
  add_library(LibfreeradiusClient::LIBFREERADIUS UNKNOWN IMPORTED)
  set_target_properties(
    LibfreeradiusClient::LIBFREERADIUS
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
               "${LibfreeradiusClient_INCLUDE_DIR}")
  set_target_properties(
    LibfreeradiusClient::LIBFREERADIUS
    PROPERTIES IMPORTED_LOCATION "${LibfreeradiusClient_LIBRARY}")
endif()
