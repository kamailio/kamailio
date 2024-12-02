# https://github.com/viaduck/cmake-modules/blob/master/FindMariaDBClient.cmake
# MIT License
#
# Copyright (c) 2018 The ViaDuck Project
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

# - Try to find MariaDB client library. This matches both the "old" client library and the new C connector.
# Once found this will define
#  MariaDBClient_FOUND - System has MariaDB client library
#  MariaDBClient_INCLUDE_DIRS - The MariaDB client library include directories
#  MariaDBClient_LIBRARIES - The MariaDB client library

# includes
find_path(
  MariaDBClient_INCLUDE_DIR
  NAMES mysql.h
  PATH_SUFFIXES mariadb mysql
)

# library
set(BAK_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_SHARED_LIBRARY_SUFFIX})
find_library(
  MariaDBClient_LIBRARY
  NAMES mariadb libmariadb mariadbclient libmariadbclient mysqlclient
        libmysqlclient
  PATH_SUFFIXES mariadb mysql
)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${BAK_CMAKE_FIND_LIBRARY_SUFFIXES})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  MariaDBClient DEFAULT_MSG MariaDBClient_LIBRARY MariaDBClient_INCLUDE_DIR
)

if(MariaDBClient_FOUND)
  message(STATUS "Found MariaDB/Mysql Client: ${MariaDBClient_LIBRARY}")
  if(NOT TARGET MariaDBClient::MariaDBClient)
    add_library(MariaDBClient::MariaDBClient UNKNOWN IMPORTED)
    set_target_properties(
      MariaDBClient::MariaDBClient
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MariaDBClient_INCLUDE_DIR}"
                 IMPORTED_LOCATION "${MariaDBClient_LIBRARY}"
    )
  endif()
endif()

mark_as_advanced(MariaDBClient_INCLUDE_DIR MariaDBClient_LIBRARY)

set(MariaDBClient_LIBRARIES ${MariaDBClient_LIBRARY})
set(MariaDBClient_INCLUDE_DIRS ${MariaDBClient_INCLUDE_DIR})
