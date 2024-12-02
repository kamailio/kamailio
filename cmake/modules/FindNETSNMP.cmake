#############################################################################
# Copyright (c) 2019 Balabit
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

# - Try to find the net-snmp library
# Once done this will define
#
#  NETSNMP_FOUND - system has the net-snmp library
#  NETSNMP_CFLAGS -
#  NETSNMP_LIBS - the libraries needed to use net-snmp
#

if(NETSNMP_LIBS)
  # Already in cache, be silent
  set(NETSNMP_FIND_QUIETLY TRUE)
endif(NETSNMP_LIBS)

find_program(NETSNMP_CONFIG_BIN net-snmp-config)

if(NETSNMP_CONFIG_BIN)
  exec_program(
    ${NETSNMP_CONFIG_BIN} ARGS
    --cflags
    OUTPUT_VARIABLE _NETSNMP_CFLAGS
  )
  exec_program(
    ${NETSNMP_CONFIG_BIN} ARGS
    --libs
    OUTPUT_VARIABLE _NETSNMP_LIBS
  )
  string(REGEX REPLACE "[\"\r\n]" " " _NETSNMP_CFLAGS "${_NETSNMP_CFLAGS}")
  string(REGEX REPLACE "[\"\r\n]" " " _NETSNMP_LIBS "${_NETSNMP_LIBS}")
  set(NETSNMP_CFLAGS
      ${_NETSNMP_CFLAGS}
      CACHE STRING "CFLAGS for net-snmp lib"
  )
  set(NETSNMP_LIBS
      ${_NETSNMP_LIBS}
      CACHE STRING "linker options for net-snmp lib"
  )
  set(NETSNMP_FOUND
      TRUE
      CACHE BOOL "net-snmp is found"
  )
else()
  set(NETSNMP_FOUND
      FALSE
      CACHE BOOL "net-snmp is not found"
  )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  NETSNMP DEFAULT_MSG NETSNMP_LIBS NETSNMP_CFLAGS NETSNMP_FOUND
)

mark_as_advanced(NETSNMP_LIBS)
