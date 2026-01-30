# https://github.com/fsaric/libsentences/blob/master/cmake/FindUnistring.cmake
# - Find UNISTRING
# Find the unistring includes and library
#
#  Unistring_INCLUDE_DIRS - where to find unistr.h, etc.
#  Unistring_LIBRARIES     - unistring library.
#  Unistring_FOUND       - True if unistring found.
#
# Hints:
#   Unistring_ROOT=/path/to/unistring/installation

find_path(Unistring_INCLUDE_DIRS unistr.h DOC "Path to unistring include directory")

find_library(
  Unistring_LIBRARIES
  NAMES unistring libunistring
  DOC "Path to unistring library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Unistring DEFAULT_MSG Unistring_LIBRARIES Unistring_INCLUDE_DIRS)

# Create the Unistring::Unistring imported target
if(Unistring_FOUND)
  add_library(Unistring::Unistring UNKNOWN IMPORTED)
  set_target_properties(
    Unistring::Unistring PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${Unistring_INCLUDE_DIRS}"
                                    IMPORTED_LOCATION "${Unistring_LIBRARIES}"
  )
endif()
mark_as_advanced(Unistring_LIBRARIES Unistring_INCLUDE_DIRS)
