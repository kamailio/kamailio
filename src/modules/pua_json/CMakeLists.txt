find_package(json-c QUIET)
if(NOT json-c_FOUND)
  message(STATUS "json-c not found. looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(json-c REQUIRED IMPORTED_TARGET json-c)
  add_library(json-c::json-c ALIAS PkgConfig::json-c)
endif()

file(GLOB SRC_FILES "*.c")

add_library(pua_json SHARED ${SRC_FILES})
target_link_libraries(pua_json PRIVATE json-c::json-c)

target_compile_options(pua_json PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pua_json/=")
