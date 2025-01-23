find_package(PkgConfig)

# TODO: Discuss if other libraries are still used
# - radiusclient library (probaly yes)
# - radiusclient-ng library (probaly not)
# Old makefile just defined the compile definitions whithout checking for
# the library.
# Modern cmake should check for the library and define the compile
# definitions if the library is found.
# Latest OS (Ubuntu) only offer radcli from package managemnt

# Mutually exclusive options

set(RADIUSCLIENTS FREERADIUS RADCLI RADIUSCLIENT_NG)
set(RADIUSCLIENT
    RADCLI
    CACHE STRING "Radius Client to use")
set_property(CACHE RADIUSCLIENT PROPERTY STRINGS ${RADIUSCLIENTS})

# option(FREERADIUS "Use freeradius-client library" OFF)
# option(RADCLI "Use radcli library" ON)
# option(RADIUSCLIENT_NG "Use radiusclient-ng library" OFF)

# if(FREERADIUS AND RADCLI)
#   message(FATAL_ERROR "Only one of FREERADIUS or RADCLI can be enabled")
# endif()

# Verify that the user-defined RADIUSCLIENT is valid
if(NOT RADIUSCLIENT IN_LIST RADIUSCLIENTS)
  message(
    FATAL_ERROR
      "Invalid RADIUSCLIENT specified: ${RADIUSCLIENT}. Available options are: ${RADIUSCLIENTS}."
  )
endif()

if(${RADIUSCLIENT} STREQUAL "FREERADIUS")
  # - freeradius-client library
  set(RADIUSCLIENT_LIB USE_FREERADIUS)
  find_package(LibfreeradiusClient REQUIRED)
  add_library(RadiusClient::RadiusClient ALIAS
              LibfreeradiusClient::LIBFREERADIUS)
elseif(${RADIUSCLIENT} STREQUAL "RADCLI")
  # - radcli library
  set(RADIUSCLIENT_LIB USE_RADCLI)
  pkg_check_modules(RADIUS REQUIRED IMPORTED_TARGET radcli)
  add_library(RadiusClient::RadiusClient ALIAS PkgConfig::RADIUS)
  # Check for radiusclient-ng version
  # elseif(NOT radiusclient_ng STREQUAL "4")
  #   # - radiusclient-ng v5 or v4 library
  #   set(RADIUSCLIENT_LIB "radiusclient-ng")
  # Uncomment if needed
  # target_compile_definitions(MyModule PRIVATE RADIUSCLIENT_NG_5P)
endif()
