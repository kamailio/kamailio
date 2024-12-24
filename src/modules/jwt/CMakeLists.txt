file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(libjwt 1.12 QUIET)
if(NOT libjwt_FOUND)
  message(STATUS "libjwt not found... looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(jwt REQUIRED IMPORTED_TARGET libjwt>=1.12)
  add_library(jwt::libjwt ALIAS PkgConfig::jwt)
endif()

target_link_libraries(${module_name} PRIVATE jwt::libjwt)
