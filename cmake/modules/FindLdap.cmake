# https://github.com/KDE/kldap/blob/master/cmake/FindLdap.cmake
# .rst:
# FindLdap
# --------
#
# Try to find the LDAP client libraries.
#
# This will define the following variables:
#
# ``Ldap_FOUND``
#     True if libldap is available.
#
# ``Ldap_VERSION``
#     The version of libldap
#
# ``Ldap_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if
#     the target is not used for linking
#
# ``Ldap_LIBRARIES``
#     The LDAP libraries (libldap + liblber if available)
#     This can be passed to target_link_libraries() instead of
#     the ``Ldap::Ldap`` target
#
# If ``Ldap_FOUND`` is TRUE, the following imported target
# will be available:
#
# ``Ldap::Ldap``
#     The LDAP library
#
# Since pre-5.0.0.
#
# Imported target since 5.1.41
#
#=============================================================================
# SPDX-FileCopyrightText: 2006 Szombathelyi György <gyurco@freemail.hu>
# SPDX-FileCopyrightText: 2007-2024 Laurent Montel <montel@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#=============================================================================

find_path(Ldap_INCLUDE_DIRS NAMES ldap.h)

if(APPLE)
  find_library(
    Ldap_LIBRARIES
    NAMES LDAP
    PATHS /System/Library/Frameworks /Library/Frameworks
  )
else()
  find_library(Ldap_LIBRARY NAMES ldap)
  find_library(Lber_LIBRARY NAMES lber)
endif()

if(Ldap_LIBRARY AND Lber_LIBRARY)
  set(Ldap_LIBRARIES ${Ldap_LIBRARY} ${Lber_LIBRARY})
endif()

if(EXISTS ${Ldap_INCLUDE_DIRS}/ldap_features.h)
  file(READ ${Ldap_INCLUDE_DIRS}/ldap_features.h LDAP_FEATURES_H_CONTENT)
  string(REGEX MATCH "#define LDAP_VENDOR_VERSION_MAJOR[ ]+[0-9]+"
               _LDAP_VERSION_MAJOR_MATCH ${LDAP_FEATURES_H_CONTENT}
  )
  string(REGEX MATCH "#define LDAP_VENDOR_VERSION_MINOR[ ]+[0-9]+"
               _LDAP_VERSION_MINOR_MATCH ${LDAP_FEATURES_H_CONTENT}
  )
  string(REGEX MATCH "#define LDAP_VENDOR_VERSION_PATCH[ ]+[0-9]+"
               _LDAP_VERSION_PATCH_MATCH ${LDAP_FEATURES_H_CONTENT}
  )

  string(REGEX REPLACE ".*_MAJOR[ ]+(.*)" "\\1" LDAP_VERSION_MAJOR
                       ${_LDAP_VERSION_MAJOR_MATCH}
  )
  string(REGEX REPLACE ".*_MINOR[ ]+(.*)" "\\1" LDAP_VERSION_MINOR
                       ${_LDAP_VERSION_MINOR_MATCH}
  )
  string(REGEX REPLACE ".*_PATCH[ ]+(.*)" "\\1" LDAP_VERSION_PATCH
                       ${_LDAP_VERSION_PATCH_MATCH}
  )

  set(Ldap_VERSION
      "${LDAP_VERSION_MAJOR}.${LDAP_VERSION_MINOR}.${LDAP_VERSION_PATCH}"
  )
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  Ldap
  FOUND_VAR Ldap_FOUND
  REQUIRED_VARS Ldap_LIBRARIES Ldap_INCLUDE_DIRS
  VERSION_VAR Ldap_VERSION
)

if(Ldap_FOUND AND NOT TARGET Lber::Lber)
  add_library(Lber::Lber UNKNOWN IMPORTED)
  set_target_properties(
    Lber::Lber PROPERTIES IMPORTED_LOCATION "${Lber_LIBRARY}"
  )
endif()

if(Ldap_FOUND AND NOT TARGET Ldap::Ldap)
  add_library(Ldap::Ldap UNKNOWN IMPORTED)
  set_target_properties(
    Ldap::Ldap
    PROPERTIES IMPORTED_LOCATION "${Ldap_LIBRARY}"
               INTERFACE_INCLUDE_DIRECTORIES "${Ldap_INCLUDE_DIRS}"
               INTERFACE_LINK_LIBRARIES Lber::Lber
  )
endif()

mark_as_advanced(Ldap_INCLUDE_DIRS Ldap_LIBRARY Lber_LIBRARY Ldap_LIBRARIES)

include(FeatureSummary)
set_package_properties(
  Ldap PROPERTIES
  URL "https://www.openldap.org/"
  DESCRIPTION "LDAP (Lightweight Directory Access Protocol) libraries."
)
