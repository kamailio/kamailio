# https://fossies.org/linux/syslog-ng/cmake/Modules/FindNETSNMP.cmake
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
  execute_process(COMMAND ${NETSNMP_CONFIG_BIN} --cflags OUTPUT_VARIABLE _NETSNMP_CFLAGS)
  execute_process(COMMAND ${NETSNMP_CONFIG_BIN} --libs OUTPUT_VARIABLE _NETSNMP_LIBS)
  # Strip trailing and leading whitespaces
  string(STRIP "${_NETSNMP_CFLAGS}" _NETSNMP_CFLAGS)
  string(STRIP "${_NETSNMP_LIBS}" _NETSNMP_LIBS)

  set(NETSNMP_CFLAGS
      ${_NETSNMP_CFLAGS}
      CACHE STRING "CFLAGS for net-snmp lib"
  )
  set(NETSNMP_LIBS
      ${_NETSNMP_LIBS}
      CACHE STRING "linker options for net-snmp lib"
  )
  set(NETSNMP_FOUND TRUE BOOL "net-snmp is found")

  add_library(NETSNMP::NETSNMP INTERFACE IMPORTED)
  set_target_properties(
    NETSNMP::NETSNMP PROPERTIES COMPILE_FLAGS "${NETSNMP_CFLAGS}" INTERFACE_LINK_LIBRARIES
                                                                  "${NETSNMP_LIBS}"
  )

  if(NOT TARGET NETSNMP::NETSNMP)
    message(
      FATAL_ERROR "Failed to create NETSNMP::NETSNMP target, check the output of net-snmp-config"
    )
  endif()
else()
  set(NETSNMP_FOUND FALSE /BOOL "net-snmp is not found")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  NETSNMP DEFAULT_MSG NETSNMP_CONFIG_BIN NETSNMP_LIBS NETSNMP_CFLAGS
)

mark_as_advanced(NETSNMP_LIBS NETSNMP_CFLAGS)
